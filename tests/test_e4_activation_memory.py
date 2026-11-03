from copy import deepcopy
from unittest.mock import patch

import torch
import torch.distributed as dist
import torch.multiprocessing as mp
from torch.nn.parallel import DistributedDataParallel

from train import e4_stage12 as stage
from train.model import ModelConfig


def inputs(batch=3, entities=47):
    return dict(tags=torch.randint(1, 10, (batch, 4, entities)),
                ids=torch.randint(1, 100, (batch, 4, entities)),
                values=torch.randn(batch, 6, entities),
                entity_mask=torch.arange(entities)[None, :] < torch.arange(batch)[:, None] + entities - batch + 1,
                candidate_kinds=torch.ones(batch, 7, dtype=torch.long),
                candidate_refs=torch.randint(0, entities + 1, (batch, 3, 7)),
                candidate_mask=torch.ones(batch, 7, dtype=torch.bool))


def direct(layer, entities, *, src_key_padding_mask, use_reentrant):
    return layer(entities, src_key_padding_mask=src_key_padding_mask)


def loss(model, data):
    logits, value = model(data)
    return logits.square().mean() + value.square().mean()


def test_recomputation_preserves_outputs_gradients_adam_and_rng(tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(143)
    model = stage.CLSPolicy(ModelConfig(48, 6, 6, 192)).train()
    reference = deepcopy(model)
    data = inputs()
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-5, eps=1e-5)
    expected_optimizer = torch.optim.Adam(reference.parameters(), lr=1e-5, eps=1e-5)
    for _ in range(2):
        optimizer.zero_grad(set_to_none=True)
        expected_optimizer.zero_grad(set_to_none=True)
        before = torch.get_rng_state()
        actual = loss(model, data)
        actual.backward()
        assert torch.equal(torch.get_rng_state(), before)
        with patch.object(stage, 'checkpoint', direct):
            expected = loss(reference, data)
            expected.backward()
        torch.testing.assert_close(actual, expected, rtol=0, atol=0)
        for a, b in zip(model.parameters(), reference.parameters()):
            torch.testing.assert_close(a.grad, b.grad, rtol=0, atol=0)
        optimizer.step()
        expected_optimizer.step()
    path = tmp_path / 'model.pt'
    torch.save(dict(model=model.state_dict(), optimizer=optimizer.state_dict()), path)
    saved = torch.load(path, weights_only=True)
    reference.load_state_dict(saved['model'], strict=True)
    expected_optimizer.load_state_dict(saved['optimizer'])
    for key, value in model.state_dict().items():
        torch.testing.assert_close(value, reference.state_dict()[key], rtol=0, atol=0)
    model.eval()
    with patch.object(stage, 'checkpoint', side_effect=AssertionError('evaluation must not recompute')):
        with torch.no_grad():
            model(data)


def test_encoder_retains_less_activation_storage():
    torch.set_num_threads(1)
    model = stage.CLSPolicy(ModelConfig(48, 6, 6, 192)).train()
    data = inputs(batch=4, entities=96)
    parameters = {p.untyped_storage().data_ptr() for p in model.parameters()}

    def retained():
        storage = {}
        def pack(tensor):
            allocation = tensor.untyped_storage()
            if allocation.data_ptr() not in parameters:
                storage[allocation.data_ptr()] = allocation.nbytes()
            return tensor
        with torch.autograd.graph.saved_tensors_hooks(pack, lambda tensor: tensor):
            output = model.encode(data)
        return sum(storage.values())

    actual = retained()
    with patch.object(stage, 'checkpoint', direct):
        expected = retained()
    assert actual < expected / 2, (actual, expected)
    print(dict(recomputed_saved_activation_bytes=actual, original_saved_activation_bytes=expected))


def ddp_worker(rank, rendezvous):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=rendezvous, rank=rank, world_size=2)
    try:
        torch.manual_seed(143)
        model = DistributedDataParallel(stage.CLSPolicy(ModelConfig(24, 2, 6, 96)))
        reference = DistributedDataParallel(deepcopy(model.module))
        torch.manual_seed(144 + rank)
        data = inputs(batch=2, entities=17)
        for step in range(2):
            for candidate, recompute in ((model, True), (reference, False)):
                candidate.zero_grad(set_to_none=True)
                with patch.object(stage, 'checkpoint', stage.checkpoint if recompute else direct):
                    with candidate.no_sync():
                        loss(candidate, data).backward()
                    loss(candidate, data).backward()
            for a, b in zip(model.parameters(), reference.parameters()):
                torch.testing.assert_close(a.grad, b.grad, rtol=0, atol=0)
    finally:
        dist.destroy_process_group()


def test_two_rank_ddp_expert_accumulation_then_ppo_sync(tmp_path):
    mp.spawn(ddp_worker, args=((tmp_path / 'gloo').as_uri(),), nprocs=2, join=True)
