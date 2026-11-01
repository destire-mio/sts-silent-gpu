import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel
from train.e4_stage12 import (CLSPolicy, NoSLSearch, NoSLCollector, new_cursor,
    advance_minibatch, student_log_baseline, distillation_terms, finish_metrics)
from train.model import ModelConfig


def test_teacher_kl_direction_and_masked_padding():
    teacher = torch.tensor([[2., -1., -torch.inf]])
    student = torch.tensor([[-.5, .5, -torch.inf]], requires_grad=True)
    legal = torch.tensor([[True, True, False]])
    kl, mse = distillation_terms(student, torch.tensor([.4], requires_grad=True),
                                teacher, torch.tensor([.7]), legal)
    expected = torch.distributions.kl_divergence(
        torch.distributions.Categorical(logits=teacher), torch.distributions.Categorical(logits=student))
    torch.testing.assert_close(kl, expected)
    torch.testing.assert_close(mse, torch.tensor([.09]))
    (kl + .5 * mse).sum().backward()
    assert torch.isfinite(student.grad).all()
    assert student.grad[0, 2] == 0


def test_large_student_learning_and_partial_cursor_resume(tmp_path):
    torch.set_num_threads(1)
    dist.init_process_group('gloo', init_method=(tmp_path/'gloo').as_uri(), rank=0, world_size=1)
    try:
        teacher = CLSPolicy().eval().requires_grad_(False)
        student = CLSPolicy(ModelConfig(384,6,6,1536))
        assert sum(p.numel() for p in student.parameters()) > sum(p.numel() for p in teacher.parameters())
        search = NoSLSearch(2, torch.device('cpu'), 7)
        collector = NoSLCollector(search, torch.Generator().manual_seed(13))
        fragment = collector.collect(teacher, length=2)
        student_log_baseline(student, fragment)
        rng = torch.Generator().manual_seed(3)
        cursor = new_cursor(fragment, rng, 1)
        model = DistributedDataParallel(student)
        optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
        scaler = torch.amp.GradScaler('cuda', enabled=False)
        before = student.cls.detach().clone()
        order=[]
        def teacher_first(module, inputs, output):
            assert not torch.is_grad_enabled()
            order.append('teacher')
        teacher_hook=teacher.register_forward_hook(teacher_first)
        student_hook=model.register_forward_hook(lambda *args:order.append('student'))
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1, teacher=teacher)
        assert order==['teacher','student']
        teacher_hook.remove();student_hook.remove()
        assert cursor['epoch'] == 1 and cursor['optimizer_steps'] == 1
        assert not torch.equal(before, student.cls)
        assert all(p.grad is None for p in teacher.parameters())
        torch.save(dict(model=student.state_dict(), optimizer=optimizer.state_dict(), cursor=cursor), tmp_path/'resume.pt')
        saved = torch.load(tmp_path/'resume.pt', weights_only=False)
        restored = CLSPolicy(ModelConfig(384,6,6,1536))
        restored.load_state_dict(saved['model'])
        resumed = DistributedDataParallel(restored)
        resumed_optimizer = torch.optim.Adam(resumed.parameters(), lr=1e-4, eps=1e-5)
        resumed_optimizer.load_state_dict(saved['optimizer'])
        advance_minibatch(resumed, resumed_optimizer, scaler, saved['cursor'], rng, 0, 1, teacher=teacher)
        row = finish_metrics(saved['cursor'], resumed, scaler)
        assert saved['cursor']['epoch'] == 2 and row['optimizer_steps'] == 2 and row['finite']
        assert all(torch.isfinite(p).all() for p in restored.parameters())
    finally:
        dist.destroy_process_group()


def test_greedy_evaluation_does_not_consume_policy_rng():
    torch.set_num_threads(1)
    search = NoSLSearch(2, torch.device('cpu'), 9, greedy=True)
    collector = NoSLCollector(search, torch.Generator().manual_seed(3))
    collector.fresh([0,1])
    search.mainline_progress = search.progress_for([0,1])
    before = search.rng.get_state().clone()
    policy = CLSPolicy().eval()
    a = search.predict(policy, [0,1])
    b = search.predict(policy, [0,1])
    assert a == b and torch.equal(before, search.rng.get_state())


def test_student_initialization_retains_collectors_and_zero_adam(tmp_path):
    from tools.e4_large_prepare import prepare
    source=tmp_path/'small';source.mkdir()
    model=CLSPolicy()
    torch.save(dict(model=model.state_dict(),optimizer={},scaler={'scale':16384.},
        state=dict(update=1900,checkpoint_serial=59,training_seconds=98000.,next_evaluation=100800.,
                   stop_reason='allocation_end_or_yield'),ppo_rng=torch.Generator().get_state()),source/'model.pt')
    for rank in range(8):
        torch.save(dict(update=(1900,59),cursor={'optimizer_steps':5},
            collector=dict(partial={'unfinished':True},completed=[1],decisions=300,
                midstart=dict(pools=[['boss'],['act2']]),rewards=['ledger'],seed_rng=rank),
            round_state={},torch_rng=torch.get_rng_state()),source/f'rank-{rank}.pt')
    output=tmp_path/'large'
    report=prepare(source,output,source/'dev-16.pt',384,53.5)
    saved=torch.load(output/'model.pt',weights_only=False)
    assert report['collectors']==8 and saved['optimizer']['state']=={}
    assert saved['state']['model_config']==dict(width=384,layers=6,heads=6,feedforward=1536)
    assert saved['state']['training_seconds']==98000. and saved['state']['distillation_updates']==0
    assert saved['state']['microbatch']==512
    for rank in range(8):
        item=torch.load(output/f'rank-{rank}.pt',weights_only=False)
        assert item['update']==(0,60) and item['cursor'] is None and item['collector']['partial'] is None
        assert item['collector']['midstart']['pools']==[['boss'],['act2']] and item['collector']['rewards']==['ledger']
    assert torch.load(source/'rank-0.pt',weights_only=False)['cursor']['optimizer_steps']==5
    from tools.e4_stage12_chain import checkpoint_state
    parent=tmp_path/'large-parent';parent.mkdir()
    output.rename(parent/'training')
    resume=checkpoint_state(dict(training_root=str(parent)))
    assert resume['optimizer_steps']==0 and resume['partial_steps']==0
