"""CPU-only comparison of the current and prepared larger CLS architectures."""
import argparse
from dataclasses import asdict
import json
from pathlib import Path
import time
import torch
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.manual_seed(129)
    batch, entities, candidates = 512, 47, 16
    inputs = dict(tags=torch.zeros(batch,4,entities,dtype=torch.long),
        ids=torch.zeros(batch,4,entities,dtype=torch.long), values=torch.ones(batch,6,entities),
        entity_mask=torch.ones(batch,entities,dtype=torch.bool),
        candidate_kinds=torch.ones(batch,candidates,dtype=torch.long),
        candidate_refs=torch.ones(batch,3,candidates,dtype=torch.long),
        candidate_mask=torch.ones(batch,candidates,dtype=torch.bool))
    results = []
    for config in (ModelConfig(384,6,6,1536), ModelConfig(768,12,12,3072)):
        policy = CLSPolicy(config)
        before = time.perf_counter()
        logits, value = policy(inputs)
        (logits.square().mean() + value.square().mean()).backward()
        elapsed = time.perf_counter() - before
        assert all(p.grad is not None and torch.isfinite(p.grad).all() for p in policy.parameters())
        parameters = sum(p.numel() for p in policy.parameters())
        tokens = entities + 1
        model_adam_gradient_bytes = 16 * parameters
        checkpoint_inputs = 4 * batch * tokens * config.width * (config.layers + 1)
        block_workspace = batch * (32*tokens*config.width + 16*config.heads*tokens*tokens
                                   + 8*tokens*config.feedforward)
        heads_workspace = 80 * batch * candidates * config.width
        activation_estimate = checkpoint_inputs + block_workspace + heads_workspace
        estimate = model_adam_gradient_bytes + activation_estimate
        results.append(dict(model=asdict(config),parameters=parameters,
            forward_backward_seconds=elapsed, model_adam_gradient_bytes=model_adam_gradient_bytes,
            activation_estimate_bytes=activation_estimate, estimated_component_total_bytes=estimate,
            capacity_reference_bytes=16*1024**3, component_estimate_below_16GiB=estimate < 16*1024**3))
        del policy, logits, value
    args.output.write_text(json.dumps(dict(device='cpu',dtype='float32',threads=1,microbatch=batch,
        entities=entities,candidates=candidates,measurements=results,
        ratio=results[1]['forward_backward_seconds']/results[0]['forward_backward_seconds'],
        actual_GPU_runs=0,synthetic_input=True,
        memory_scope='FP32 model/grad/Adam plus checkpointed activations and one layer/head workspace; estimate only for the stated shape, excludes rollout/native/cache/teacher/CUDA workspace; not a GPU peak measurement'),indent=2)+'\n')


if __name__ == '__main__':
    main()
