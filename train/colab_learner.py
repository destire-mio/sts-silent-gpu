"""Opt-in encoder optimizations for the standalone Stage12 learner."""
from functools import wraps
import warnings

import torch
from torch.nn.attention import SDPBackend, sdpa_kernel


def configure_learner(policy, *, compile_encoder=False, fused_sdpa=False):
    """Keep parameter names and the eager evaluation path intact."""
    fallbacks = set()
    if not (compile_encoder or fused_sdpa):
        return fallbacks

    def learner_forward(layer, eager, optimized):
        @wraps(eager)
        def forward(*args, **kwargs):
            if not layer.training or not torch.is_grad_enabled():
                return eager(*args, **kwargs)
            if not fused_sdpa:
                return optimized(*args, **kwargs)
            try:
                with sdpa_kernel([SDPBackend.FLASH_ATTENTION, SDPBackend.EFFICIENT_ATTENTION]):
                    return optimized(*args, **kwargs)
            except RuntimeError as error:
                message = str(error).lower()
                if not any(x in message for x in ("no available kernel", "no viable backend", "no suitable kernel")):
                    raise
                reason = str(error).splitlines()[0]
                if reason not in fallbacks:
                    fallbacks.add(reason)
                    warnings.warn(f"Fused SDPA unavailable for this input. {reason}", RuntimeWarning, stacklevel=2)
                return optimized(*args, **kwargs)
        return forward

    for layer in policy.encoder:
        eager = layer.forward
        optimized = torch.compile(eager, dynamic=True) if compile_encoder else eager
        layer.forward = learner_forward(layer, eager, optimized)
    return fallbacks
