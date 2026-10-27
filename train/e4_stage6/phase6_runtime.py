from dataclasses import asdict
import gc
import gzip
import json
from pathlib import Path
import time

import torch
import torch.distributed as dist

from train.e4 import RoomSearch, CAPACITY_METRICS, LOOP_METRICS, SEARCH_METRICS, combat_diagnostics_summary
from train.e4_rewards import REST_OPTIONS, noncombat_summary
from train.model import ModelConfig
from tools.e4_run import atomic_json, audit, summaries
from train.e4_stage6.construction import create_training_worker
from train.e4_stage6.combat import CombatPolicy
from train.e4_stage6.evaluation_worker import EvaluationWorker, policy_hash
from train.e4_stage6.exam_statistics import compare
from train.model import Policy
from train.e4_stage6.phase6_controller import Phase6Schedule, run_training_slice
from train.e4_stage6.phase6_storage import Phase6Storage, file_digest
from train.e4_stage6.training_worker import SEARCH_STATE


def decision_metrics(episodes):
    rows = [e['noncombat']['counts'] for e in episodes]
    total = lambda name: sum(row.get(name,0) for row in rows)
    rest = {name:total('rest_'+name) for name in REST_OPTIONS}
    rests, choices, shops = sum(rest.values()),total('card_reward_choices'),total('shop_rooms')
    return dict(relics_taken=total('relic_taken'), relics_appeared=total('relic_appeared'),
        sapphire_key_selected=total('sapphire_key_selected'), ruby_key_selected=total('ruby_key_selected'),
        rest_options=rest, rest_option_fractions={key:value/rests if rests else None for key,value in rest.items()},
        shop_rooms=shops, shop_opened=total('shop_opened'), shop_entry_rate=total('shop_opened')/shops if shops else None,
        shop_purchases=total('shop_purchases'), card_skipped=total('card_skipped'), card_reward_choices=choices,
        card_skip_rate=total('card_skipped')/choices if choices else None,
        out_of_combat_potion_discards=total('out_of_combat_potion_discards'),
        out_of_combat_potion_discards_by_seed=[dict(seed=e['seed'],count=row.get('out_of_combat_potion_discards',0))
                                             for e,row in zip(episodes,rows)],
        scope='Actual completed mainline trajectories; absent count keys are zero')


class Phase6Runtime:
    def __init__(self, root, binding, source_paths, source_hashes, exam, evaluation_anchor,
                 baseline, device, stop_file, job_id):
        self.root, self.device, self.stop_file = Path(root), device, Path(stop_file)
        self.binding, self.exam, self.job_id = binding, exam, str(job_id)
        self.rank, self.world = dist.get_rank(), dist.get_world_size()
        assert binding['world'] == self.world and self.world in (7, 8)
        assert len(source_paths) == len(source_hashes) == 8
        assert binding['source_hashes'] == source_hashes
        assert not exam['training_eligible'] and exam['evaluation_accepted']
        assert sum(map(len, exam['groups'].values())) == 1490
        assert len(baseline) == 1024 and {e['seed'] for e in baseline} == set(range(2000000000, 2000001024))
        self.source_path = Path(source_paths[self.rank])
        self.source_sha256 = source_hashes[self.rank]
        self.evaluation_anchor, self.baseline = evaluation_anchor, baseline
        self.store = Phase6Storage(root, binding)
        self.schedule = Phase6Schedule(binding)
        self.state = self.store.load_state()
        if self.state is None:
            self.state = dict(training={}, evaluations={}, boundary={'A':0., 'B':0.},
                              schedule=self.schedule.state, stopped_reason=None, evaluation_node_seconds=0.)
        else:
            self.schedule.restore(self.state['schedule'])
        self.worker = None

    def save(self):
        self.state['schedule'] = self.schedule.state
        self.store.save_state(self.state)

    def stopped(self, deadline):
        value = torch.tensor(int(self.stop_file.exists() or time.monotonic() >= deadline), device=self.device)
        dist.all_reduce(value, op=dist.ReduceOp.MAX)
        return bool(value)

    def drop_worker(self):
        self.worker = None
        gc.collect()

    def source(self):
        assert file_digest(self.source_path) == self.source_sha256
        source = torch.load(self.source_path, map_location='cpu', weights_only=False)
        source['state'] = {key:source['state'][key] for key in
            ('phase','updates','imitation_beta','imitation_entropy_target','training_rng','train_state')}
        source['state']['train_state'] = {key:source['state']['train_state'][key] for key in ('seed_rng','search_rng')}
        return {key:source[key] for key in ('commit','task','world_size','config','model','optimizer','ppo_rng','state')}

    def trainer(self, arm):
        assert self.worker is None
        source = self.source()
        self.worker = create_training_worker(source, self.source_sha256, self.binding['runtime_commit'],
            arm, self.device, self.root, self.stop_file)
        del source
        if arm in self.state['training']:
            saved = self.store.load_worker(self.state['training'][arm])
            self.worker.restore(saved)
            del saved
        return self.worker

    def evaluator(self, arm, checkpoint, evaluation_reference=None):
        assert self.worker is None
        saved = self.store.load_worker(checkpoint)
        config = ModelConfig(**saved['binding']['config'])
        policy = (Policy if arm == 'A' else CombatPolicy)(config).to(self.device)
        policy.load_state_dict(saved['model'], strict=True)
        del saved
        rng = torch.Generator(device=self.device)
        rng.set_state(self.evaluation_anchor['search_rng'])
        seed_rng = torch.Generator()
        seed_rng.set_state(self.evaluation_anchor['seed_rng'])
        search = RoomSearch(256, 32, self.device, rng, scratch_slots=256*33, bosses_seen=511)
        search.evidence_directory = self.root
        binding = dict(arm=arm, rank=self.rank, world=self.world, runtime_commit=self.binding['runtime_commit'],
            checkpoint_sha256=checkpoint['manifest_sha256'], policy_sha256=policy_hash(policy), config=asdict(config))
        self.worker = EvaluationWorker(policy, search, seed_rng, self.evaluation_anchor['search_rng'],
                                       self.exam, binding, self.stop_file)
        if evaluation_reference is not None:
            saved = self.store.load_worker(evaluation_reference)
            self.worker.restore(saved)
            del saved
        return self.worker

    def train(self, action, deadline):
        arm, round_number = action['arm'], action['round']
        worker = self.trainer(arm)
        report = run_training_slice(worker, action['target_node_seconds'], deadline, self.stop_file,
                                    self.state['boundary'][arm])
        reference = self.store.save_worker(worker, 'training', round_number)
        self.state['training'][arm] = reference
        self.state['boundary'][arm] = report['boundary_node_seconds']
        self.schedule.record_training(arm, report, reference)
        if report['reason'] == 'training_health_stop':
            self.state['stopped_reason'] = report['reason']
        self.save()
        del worker
        self.drop_worker()
        return report['finished']

    def evaluate(self, action, deadline):
        arm, round_number, checkpoint = action['arm'], action['round'], action['checkpoint']
        key = f'{arm}:{round_number}'
        reference = self.state['evaluations'].get(key)
        worker = self.evaluator(arm, checkpoint, reference)
        full = action['action'] == 'full_run_1024'
        if full and worker.state['phase'] == 'examination_complete':
            worker.begin_full_run()
        assert worker.state['phase'] in (('full_run','complete') if full else ('examination','examination_complete'))
        target = 'complete' if full else 'examination_complete'
        while True:
            completed = torch.tensor(int(worker.state['phase'] == target), device=self.device)
            dist.all_reduce(completed, op=dist.ReduceOp.MIN)
            if bool(completed):
                break
            if self.stopped(deadline):
                break
            if worker.state['phase'] != target:
                worker.step(deadline)
        reference = self.store.save_worker(worker, 'evaluation', round_number)
        self.state['evaluations'][key] = reference
        completed = torch.tensor(int(worker.state['phase'] == target), device=self.device)
        dist.all_reduce(completed, op=dist.ReduceOp.MIN)
        finished = bool(completed)
        if finished:
            record = (self.write_full_run(worker, action, reference) if full
                      else self.write_examination(worker, action, reference))
            if full:
                self.schedule.record_full_run(arm, record)
            else:
                self.schedule.record_examination(arm, record)
        self.save()
        del worker
        self.drop_worker()
        return finished

    def write_rank_rows(self, reference, name, rows, diagnostics):
        directory = self.root / reference['directory']
        atomic_json(directory/f'{name}-rank-{self.rank}.json', rows)
        atomic_json(directory/f'{name}-diagnostics-{self.rank}.json', diagnostics)
        dist.barrier()
        if self.rank != 0:
            return None
        return sum([json.loads((directory/f'{name}-rank-{rank}.json').read_bytes())
                    for rank in range(self.world)], [])

    def write_examination(self, worker, action, reference):
        diagnostics = {key:getattr(worker.search,key) for key in SEARCH_STATE if key != 'available'}
        rows = self.write_rank_rows(reference, 'examination', worker.cursor['rows'], diagnostics)
        record = None
        if self.rank == 0:
            scenes = {(s['seed'],tuple(s['prefix'])) for group in self.exam['groups'].values() for s in group}
            expected = {(seed,prefix,mode,repetition) for seed,prefix in scenes
                        for mode,repetition in [('replay32',0),('single',0),('single',1),('single',2),('single',3)]}
            actual = {(r['seed'],tuple(r['prefix']),r['mode'],r['repetition']) for r in rows}
            assert len(rows) == len(actual) == 1490*5 and actual == expected
            path = self.root/reference['directory']/'examination.json'
            atomic_json(path, rows)
            record = dict(complete=True, checkpoint=action['checkpoint'], rows=7450,
                reference=reference, results_sha256=file_digest(path), evaluation_only=True)
        return self.store.broadcast(record)

    def write_full_run(self, worker, action, reference):
        name = f"E4-C89-stage6-{self.binding['run_id']}-job{self.job_id}-{action['arm']}-R{action['round']}-N32-D1024"
        diagnostics = {key:getattr(worker.search,key) for key in SEARCH_STATE if key != 'available'}
        episodes = self.write_rank_rows(reference, name, worker.episodes, diagnostics)
        record = None
        if self.rank == 0:
            assert len(episodes) == 1024 and {e['seed'] for e in episodes} == set(range(2000000000,2000001024))
            episodes.sort(key=lambda e:e['seed'])
            directory = self.root/reference['directory']
            counts = [json.loads((directory/f'{name}-diagnostics-{rank}.json').read_bytes())
                      for rank in range(self.world)]
            search_summary = {key:sum(row['metrics'][key] for row in counts) for key in SEARCH_METRICS[:2]}
            search_summary.update({key:max(row['metrics'][key] for row in counts) for key in SEARCH_METRICS[2:]})
            search_summary.update({key:sum(row['capacity_metrics'][key] for row in counts) for key in CAPACITY_METRICS})
            search_summary.update({key:sum(row['loop_metrics'][key] for row in counts) for key in LOOP_METRICS})
            capped = sum(row['capacity_metrics']['combat_played_cards_limit'] for row in counts)
            trials = sum(row['capacity_metrics']['combat_trials'] for row in counts)
            fraction = capped/trials if trials else 0.
            assessment = dict(summaries(episodes), evaluation_only=True, checkpoint=action['checkpoint'],
                round=name, bosses_seen=511, trials=32, lanes=256, world_size=self.world,
                noncombat=noncombat_summary(episodes), combat_played_cards_limit_fraction=fraction,
                decision_metrics=decision_metrics(episodes),
                combat=combat_diagnostics_summary(sum([row['combat_diagnostics'] for row in counts],[])),
                **search_summary,
                faults_truncated_scope='Producer summary constants, not independent original-game proof')
            atomic_json(directory/f'{name}-summary.json', assessment)
            atomic_json(directory/f'{name}-audit.json', audit(episodes,name,action['round']+1,self.binding['runtime_commit']))
            path = directory/f'{name}-episodes.json.gz'
            with gzip.open(path.with_suffix('.gz.tmp'),'wt',encoding='utf-8') as stream:
                json.dump(episodes,stream,separators=(',',':'))
            path.with_suffix('.gz.tmp').replace(path)
            record = dict(complete=True, episodes=1024, checkpoint=action['checkpoint'], reference=reference,
                episodes_file=path.name, episodes_sha256=file_digest(path), summary=assessment)
        record = self.store.broadcast(record)
        if record['summary']['combat_played_cards_limit_fraction'] > .001:
            self.state['stopped_reason'] = 'combat_played_cards_limit_above_one_per_thousand'
        return record

    def comparison(self):
        result = None
        if self.rank == 0:
            s = self.schedule.state
            exams, episodes = [], []
            for arm in ('A','B'):
                matches = [e for e in s['examinations'] if e['arm'] == arm and e['round'] == s['last_matched']['round']]
                assert len(matches) == 1 and matches[0]['checkpoint'] == s['last_matched']['checkpoints'][arm]
                path = self.root/matches[0]['reference']['directory']/'examination.json'
                assert file_digest(path) == matches[0]['results_sha256']
                exams.append(json.loads(path.read_bytes()))
                record = s['full_runs'][arm]
                path = self.root/record['reference']['directory']/record['episodes_file']
                assert file_digest(path) == record['episodes_sha256']
                with gzip.open(path,'rt',encoding='utf-8') as stream:
                    episodes.append(json.load(stream))
            result = compare(*exams,*episodes,self.baseline)
            atomic_json(self.root/'phase6-comparison.json', result)
        return self.store.broadcast(result)

    def run(self, allocation_clock, examination_reserve_remaining, overhead_reserve):
        if self.state['stopped_reason'] is not None:
            return dict(reason=self.state['stopped_reason'], complete=False)
        while not self.stopped(allocation_clock.deadline):
            choice = None
            if self.rank == 0:
                action = self.schedule.next_action(allocation_clock.remaining_node_seconds(),
                    max(0.,examination_reserve_remaining-self.state['evaluation_node_seconds']), overhead_reserve)
                choice = (action,self.schedule.state)
            action, schedule_state = self.store.broadcast(choice)
            self.schedule.restore(schedule_state)
            self.save()
            if action['action'] == 'train':
                if not self.train(action, allocation_clock.deadline):
                    break
            elif action['action'] in ('examine','full_run_1024'):
                started = time.monotonic()
                finished = self.evaluate(action, allocation_clock.deadline)
                elapsed = torch.tensor(time.monotonic()-started, dtype=torch.float64, device=self.device)
                dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
                self.state['evaluation_node_seconds'] += float(elapsed)*self.world/8
                self.save()
                if not finished or self.state['stopped_reason'] is not None:
                    break
                if self.schedule.state['phase'] == 'complete':
                    return dict(reason='complete', complete=True, comparison=self.comparison())
            elif action['action'] == 'select_last_matched_for_evaluation':
                self.schedule.select_last_matched()
                self.save()
            elif action['action'] == 'complete':
                return dict(reason='complete', complete=True, comparison=self.comparison())
            else:
                assert action['action'] == 'no_balanced_training'
                return dict(reason='no_balanced_training', complete=False)
        self.save()
        return dict(reason=self.state['stopped_reason'] or 'parent_stop_or_allocation_deadline', complete=False)
