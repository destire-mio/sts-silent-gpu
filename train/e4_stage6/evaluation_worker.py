from copy import deepcopy
import hashlib
import time

import torch
import torch.distributed as dist

from train.e4 import AllocationEnded, SegmentCollector
from train.e4_stage6.construction import bind_parent_stop
from train.e4_stage6.examination import begin_examination, advance_examination, validate_examination_cursor
from train.e4_stage6.training_worker import SEARCH_STATE, move_tensors


def policy_hash(policy):
    digest = hashlib.sha256()
    for name, value in sorted(policy.state_dict().items()):
        digest.update(name.encode())
        digest.update(value.detach().cpu().contiguous().numpy().tobytes())
    return digest.hexdigest()


class EvaluationWorker:
    def __init__(self, policy, search, seed_rng, full_run_rng, exam, binding, stop_file):
        assert binding['arm'] in ('A','B')
        assert binding['rank'] == dist.get_rank() and binding['world'] == dist.get_world_size()
        assert binding['policy_sha256'] == policy_hash(policy)
        self.policy = policy.eval().requires_grad_(False)
        self.search, self.seed_rng, self.full_run_rng = search, seed_rng, full_run_rng.clone()
        self.exam, self.binding = exam, deepcopy(binding)
        self.rank, self.world = binding['rank'], binding['world']
        self.device = search.device
        self.inference = policy if binding['arm'] == 'A' else policy.run_policy
        self.cursor = begin_examination(exam, search, binding['policy_sha256'], self.rank, self.world)
        self.search.combat_diagnostics = []
        self.search.step0_fallback.clear()
        self.search.step0_fallback_records.clear()
        self.collector = None
        self.state = dict(phase='examination', examination_seconds=0., full_run_seconds=0., interrupted_calls=0,
                          examination_search=None)
        bind_parent_stop(search, stop_file)

    @property
    def episodes(self):
        return self.collector.completed if self.collector is not None else []

    @torch.no_grad()
    def step(self, deadline):
        assert policy_hash(self.policy) == self.binding['policy_sha256'], 'Frozen evaluation policy changed'
        if self.state['phase'] in ('examination_complete','complete'):
            return dict(action=self.state['phase'], performed_evaluation=False)
        self.search.deadline = deadline
        try:
            self.search.clock()
        except AllocationEnded:
            return dict(action='stopped', performed_evaluation=False)
        phase = self.state['phase']
        started = time.monotonic()
        expired = False
        try:
            if phase == 'examination':
                if not self.cursor['done']:
                    advance_examination(self.cursor, self.search, self.inference)
            else:
                assert phase == 'full_run'
                seeds = list(range(2000000000+self.rank, 2000001024, self.world))
                episodes = self.collector.evaluate(self.inference, seeds)
                assert sorted(e['seed'] for e in episodes) == seeds
        except AllocationEnded:
            expired = True
            self.state['interrupted_calls'] += 1
        elapsed = time.monotonic() - started
        self.state['examination_seconds' if phase == 'examination' else 'full_run_seconds'] += elapsed
        assert policy_hash(self.policy) == self.binding['policy_sha256'], 'Frozen evaluation policy changed'
        if expired:
            return dict(action='interrupted', performed_evaluation=True, seconds=elapsed)
        if phase == 'examination' and self.cursor['done']:
            assert len(self.cursor['rows']) == len(self.cursor['scenes']) * 5
            self.state['examination_search'] = deepcopy({key:getattr(self.search,key)
                                                        for key in SEARCH_STATE if key != 'available'})
            self.state['phase'] = 'examination_complete'
        elif phase == 'full_run':
            self.state['phase'] = 'complete'
        return dict(action=self.state['phase'], performed_evaluation=True, seconds=elapsed)

    def begin_full_run(self):
        assert self.state['phase'] == 'examination_complete' and self.collector is None
        for key in ('metrics','capacity_metrics','loop_metrics'):
            setattr(self.search,key,dict.fromkeys(getattr(self.search,key),0))
        self.search.event_trial_max_steps = self.search.searched_decisions = 0
        self.search.combat_diagnostics = []
        self.search.step0_fallback.clear()
        self.search.step0_fallback_records.clear()
        self.search.rng.set_state(self.full_run_rng)
        self.collector = SegmentCollector(self.search,self.seed_rng,rank=self.rank)
        self.state['phase'] = 'full_run'

    def snapshot(self):
        assert not self.search.contexts
        assert policy_hash(self.policy) == self.binding['policy_sha256']
        collector = self.collector.state() if self.collector is not None else None
        native = ({s: hashlib.sha256(self.search.batch.state_bytes(s)).hexdigest()
                   for s, seed in enumerate(self.collector.seeds) if seed is not None}
                  if self.collector is not None else {})
        return move_tensors(dict(format=1, evaluation_only=True, binding=self.binding,
            state=self.state, model=self.policy.state_dict(), cursor=self.cursor,
            full_run_rng=self.full_run_rng, collector=collector, active_native_state_sha256=native,
            search={key:getattr(self.search,key) for key in SEARCH_STATE}), torch.device('cpu'))

    def restore(self, saved):
        assert saved['format'] == 1 and saved['evaluation_only'] and saved['binding'] == self.binding
        validate_examination_cursor(saved['cursor'], self.exam, self.search,
                                    self.binding['policy_sha256'], self.rank, self.world)
        saved = move_tensors(saved, torch.device('cpu'))
        self.policy.load_state_dict(saved['model'], strict=True)
        assert policy_hash(self.policy) == self.binding['policy_sha256']
        self.state, self.cursor, self.full_run_rng = saved['state'], saved['cursor'], saved['full_run_rng']
        for key in SEARCH_STATE:
            if key in ('step0_fallback', 'step0_fallback_records') and key not in saved['search']:
                continue
            setattr(self.search, key, saved['search'][key])
        if saved['collector'] is not None:
            self.collector = SegmentCollector(self.search, self.seed_rng, rank=self.rank)
            self.collector.restore(saved['collector'])
            actual = {s:hashlib.sha256(self.search.batch.state_bytes(s)).hexdigest()
                      for s,seed in enumerate(self.collector.seeds) if seed is not None}
            assert actual == saved['active_native_state_sha256'], 'Restored evaluation Game+History differs'
        else:
            assert self.state['phase'] in ('examination','examination_complete')
            self.collector = None
        self.policy.eval().requires_grad_(False)
