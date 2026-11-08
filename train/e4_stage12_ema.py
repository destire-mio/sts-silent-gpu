"""Exponential moving average for standalone Stage12 evaluation."""
import torch


class ExponentialWeights:
    def __init__(self, model, decay, saved=None):
        self.model = model
        self.decay = decay
        self.weights = {name: value.detach().clone() for name, value in model.state_dict().items()}
        if saved is not None:
            if saved.keys() != self.weights.keys():
                raise ValueError("EMA checkpoint keys differ from the active policy")
            for name, value in saved.items():
                self.weights[name].copy_(value)

    @torch.no_grad()
    def update(self):
        current = self.model.state_dict()
        floating = [name for name, value in self.weights.items() if value.is_floating_point()]
        torch._foreach_lerp_([self.weights[name] for name in floating],
                             [current[name] for name in floating], 1 - self.decay)
        for name, value in self.weights.items():
            if not value.is_floating_point():
                value.copy_(current[name])

    def eval(self):
        self.model.eval()

    def __call__(self, observation):
        return torch.func.functional_call(self.model, self.weights, (observation,), strict=True)
