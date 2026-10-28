from copy import deepcopy
import hashlib
import json
import random
import time

import numpy as np
import torch
import torch.distributed as dist

from train.e4 import AllocationEnded
from train.e4_learner import update_beta
from train.e4_stage6.combat import proportional_beta
from train.e4_stage6.copy_learner import begin_update, advance_update, finish_update


SEARCH_STATE = ('metrics', 'capacity_metrics', 'loop_metrics', 'event_trial_max_steps',
                'searched_decisions', 'combat_diagnostics', 'available', 'step0_fallback', 'step0_fallback_records')


def training_health(history,health):
    history=(history+[(int(health[0]),int(health[2]))])[-20:]
    completed=sum(row[0]for row in history)
    stalled=sum(row[1]for row in history)
    return history,(completed==0 and len(history)>=20)or stalled*20>completed


def move_tensors(value, device):
    if isinstance(value, torch.Tensor):
        return value.detach().to(device, copy=True)
    if isinstance(value, dict):
        return {k: move_tensors(v, device) for k, v in value.items()}
    if isinstance(value, list):
        return [move_tensors(v, device) for v in value]
    if isinstance(value, tuple):
        return tuple(move_tensors(v, device) for v in value)
    return deepcopy(value)


class TrainingWorker:
    def __init__(self, model, optimizer, collector, ppo_rng, binding, beta, entropy_target):
        assert binding['arm'] in ('A', 'B') and binding['bosses_seen'] == 511
        assert binding['rank'] == dist.get_rank() and binding['world'] == dist.get_world_size()
        assert all(group['lr'] == 1e-4 for group in optimizer.param_groups)
        self.model, self.optimizer, self.collector = model, optimizer, collector
        self.ppo_rng, self.binding = ppo_rng, deepcopy(binding)
        self.device = collector.search.device
        self.state = dict(beta=beta, entropy_target=entropy_target, updates=0,
            training_wall_seconds=0., training_node_seconds=0., update_seconds=0.,
            collection_seconds=0., learner_seconds=0., interrupted_collection_seconds=0.,
            update_collection_seconds=0., update_learner_seconds=0., rows=[],health=[],health_stopped=False)
        self.ready, self.cursor = None, None

    def synchronize(self):
        if self.device.type == 'cuda':
            torch.cuda.synchronize(self.device)

    def stopped(self, flag):
        value = torch.tensor(int(flag), device=self.device)
        dist.all_reduce(value, op=dist.ReduceOp.MAX)
        return bool(value)

    def charge_training(self, started, phase, interrupted):
        self.synchronize()
        elapsed = torch.tensor(time.monotonic() - started, dtype=torch.float64, device=self.device)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        seconds = float(elapsed)
        self.state['training_wall_seconds'] += seconds
        self.state['training_node_seconds'] += seconds * self.binding['world'] / 8
        self.state['update_seconds'] += seconds
        self.state[phase + '_seconds'] += seconds
        self.state['update_' + phase + '_seconds'] += seconds
        if interrupted:
            self.state['interrupted_collection_seconds'] += seconds
        return seconds

    def step(self, deadline):
        if self.state['health_stopped']:
            return dict(action='training_health_stop',performed_training=False)
        if self.state.get('diagnostic_stopped_reason') is not None:
            return dict(action=self.state['diagnostic_stopped_reason'],performed_training=False)
        self.collector.search.deadline = deadline
        expired = time.monotonic() >= deadline
        if not expired:
            try:
                self.collector.search.clock()
            except AllocationEnded:
                expired = True
        if self.stopped(expired):
            return dict(action='deadline', performed_training=False)
        self.synchronize()
        started = time.monotonic()
        arm = self.binding['arm']
        if self.cursor is None:
            self.model.module.eval()
            expired = False
            if self.ready is None:
                try:
                    self.ready = self.collector.collect(self.model.module)
                except AllocationEnded:
                    expired = True
            expired = self.stopped(expired)
            if not expired:
                self.cursor = begin_update(self.ready, self.ppo_rng, 4, arm)
                self.cursor['training_health']=self.ready['health'].clone()
                valid=self.ready['action_valid']
                self.cursor['training_decisions']=torch.stack((valid.new_tensor(valid.numel(),dtype=torch.int64),
                    valid.sum(),(valid & self.ready['combat']).sum(),(valid & self.ready['imitation']).sum()))
                self.ready = None
            elapsed = self.charge_training(started, 'collection', expired)
            return dict(action='collection_interrupted' if expired else 'fragment_ready',
                performed_training=True, seconds=elapsed,
                atomic_overshoot_seconds=max(0., time.monotonic() - deadline))
        self.model.train()
        advance_update(self.model, self.optimizer, self.cursor, self.ppo_rng, self.state['beta'])
        metrics = finish_update(self.cursor, self.state['beta']) if self.cursor['done'] else None
        if metrics is not None:
            health=self.cursor['training_health'].clone()
            dist.all_reduce(health[:3])
            dist.all_reduce(health[3:],op=dist.ReduceOp.MAX)
            self.state['health'],self.state['health_stopped']=training_health(self.state['health'],health)
            decisions=self.cursor['training_decisions'].clone()
            dist.all_reduce(decisions)
            metrics.update(fragment_positions_global=int(decisions[0]),
                actual_valid_mainline_decisions_global=int(decisions[1]),
                valid_combat_mainline_decisions_global=int(decisions[2]),
                valid_imitation_mainline_decisions_global=int(decisions[3]),
                retained_copy_decisions_global=metrics['copy_samples']//self.cursor['epochs'],
                decision_scope='Global fragment counts before epoch repeats; trained_rows includes epochs and is not an effective-decision count')
            metrics.update(completed_episodes=int(health[0]),stalled_episodes=int(health[2]),
                           completed_floor_sum=int(health[1]),active_max_floor_steps=int(health[3]))
        elapsed = self.charge_training(started, 'learner', False)
        if metrics is not None:
            self.state['updates'] += 1
            row = dict(metrics, update=self.state['updates'],
                training_wall_seconds=self.state['training_wall_seconds'],
                training_node_seconds=self.state['training_node_seconds'],
                update_seconds=self.state['update_seconds'],
                collection_seconds=self.state['update_collection_seconds'],
                learner_seconds=self.state['update_learner_seconds'])
            self.state['rows'].append(row)
            if arm == 'B' and metrics['H_imit'] < self.state['entropy_target'] / 2:
                self.state['diagnostic_stopped_reason'] = 'B_entropy_below_half_target'
            if self.binding['rank'] == 0:
                print(json.dumps(dict(event='stage6_update',arm=arm,
                    first_A_update_timing=arm == 'A' and self.state['updates'] == 1,**row)),flush=True)
            self.state['beta'] = (update_beta if arm == 'A' else proportional_beta)(
                self.state['beta'], self.state['entropy_target'], metrics['H_imit'])
            self.state['update_seconds'] = 0.
            self.state['update_collection_seconds'] = 0.
            self.state['update_learner_seconds'] = 0.
            self.cursor = None
        action=self.state.get('diagnostic_stopped_reason') or (
                'training_health_stop' if self.state['health_stopped'] else
                'update_complete' if metrics is not None else 'minibatch_complete')
        return dict(action=action,
            performed_training=True, seconds=elapsed, metrics=metrics,
            atomic_overshoot_seconds=max(0., time.monotonic() - deadline))

    def snapshot(self):
        search = self.collector.search
        assert not search.contexts
        collector = self.collector.state()
        tape = collector.pop('combat_tape') if self.binding['arm'] == 'B' else None
        partial_capacity = None
        if collector['partial'] is not None:
            collector = dict(collector, partial=dict(collector['partial'],
                result=dict(collector['partial']['result'])))
            result = collector['partial']['result']
            partial_capacity = result['data'].numel()
            rows = collector['partial']['next_step'] * search.lanes
            result['data'] = result['data'][:int(result['offsets'][rows])]
        ready = self.ready
        if ready is not None:
            ready = dict(ready, data=ready['data'][:int(ready['offsets'][-1])])
        rng = dict(torch=torch.get_rng_state(), python=random.getstate(), numpy=np.random.get_state(),
            cuda=torch.cuda.get_rng_state(self.device) if self.device.type == 'cuda' else None)
        states = {s: hashlib.sha256(search.batch.state_bytes(s)).hexdigest()
                  for s, seed in enumerate(self.collector.seeds) if seed is not None}
        saved = move_tensors(dict(format=1, binding=self.binding, state=self.state,
            model=self.model.module.state_dict(), optimizer=self.optimizer.state_dict(),
            model_training=self.model.module.training, ppo_rng=self.ppo_rng.get_state(),
            collector=collector, partial_data_capacity=partial_capacity, ready=ready,
            cursor=self.cursor, search={key: getattr(search, key) for key in SEARCH_STATE},
            active_native_state_sha256=states, rng=rng), torch.device('cpu'))
        if self.binding['arm'] == 'B':
            saved['collector']['combat_tape'] = tape
        return saved

    def restore(self, saved):
        assert saved['format'] == 1 and saved['binding'] == self.binding
        tape = saved['collector']['combat_tape'] if self.binding['arm'] == 'B' else None
        collector = {key:value for key,value in saved['collector'].items() if key != 'combat_tape'}
        saved = move_tensors(dict(saved,collector=collector), torch.device('cpu'))
        if self.binding['arm'] == 'B':
            saved['collector']['combat_tape'] = tape
        self.model.module.load_state_dict(saved['model'], strict=True)
        self.optimizer.load_state_dict(saved['optimizer'])
        self.ppo_rng.set_state(saved['ppo_rng'])
        collector = saved['collector']
        if saved['partial_data_capacity'] is not None:
            result = collector['partial']['result']
            data = torch.empty(saved['partial_data_capacity'], dtype=torch.uint8, device=self.device)
            data[:len(result['data'])] = result['data'].to(self.device)
            result['data'] = data
        self.collector.restore(collector)
        search = self.collector.search
        for key in SEARCH_STATE:
            if key in ('step0_fallback', 'step0_fallback_records') and key not in saved['search']:
                continue
            setattr(search, key, saved['search'][key])
        actual = {s: hashlib.sha256(search.batch.state_bytes(s)).hexdigest()
                  for s, seed in enumerate(self.collector.seeds) if seed is not None}
        assert actual == saved['active_native_state_sha256'], 'Restored native Game+History differs'
        self.state = saved['state']
        self.ready = move_tensors(saved['ready'], self.device)
        self.cursor = move_tensors(saved['cursor'], self.device)
        self.model.train(saved['model_training'])
        torch.set_rng_state(saved['rng']['torch'])
        random.setstate(saved['rng']['python'])
        np.random.set_state(saved['rng']['numpy'])
        if self.device.type == 'cuda':
            torch.cuda.set_rng_state(saved['rng']['cuda'], self.device)
