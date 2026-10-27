from math import isfinite
from pathlib import Path
import time

from train.e4_stage6.phase6_storage import file_digest


def validate_allocation(context, job_id, world, runtime_commit, now):
    assert context['format'] == 1 and context['phase'] == 'claude87_stage6'
    assert context['runtime_commit'] == runtime_commit and world in (7,8)
    receipt, reservation, release, allocation = (context[key] for key in
        ('receipt','reservation','release','allocation'))
    assert all(str(record['job_id']) == str(job_id) for record in (receipt,reservation,release,allocation))
    assert receipt['world'] == allocation['world'] == world
    assert allocation['cpus'] == world+1
    wall = allocation['wall_seconds']
    assert 0 < wall <= 14400 and receipt['wall_seconds'] == wall
    assert allocation['end_at']-allocation['start_at'] == wall
    assert 0 <= receipt['at'] <= reservation['at'] <= release['at'] <= allocation['start_at'] <= now
    for boundary in (receipt['at'],release['at']):
        assert 0 <= boundary-release['budget_at'] <= 300
        assert 0 <= boundary-release['preflight_at'] <= 300
    receipt_file = receipt['stdout']
    path = Path(receipt_file['path'])
    assert file_digest(path) == receipt_file['sha256']
    assert path.read_bytes().decode().strip() == f'Submitted batch job {job_id}'
    for record in (reservation['parent_ledger'],release['preflight'],allocation['scontrol']):
        assert file_digest(Path(record['path'])) == record['sha256']
    reserve = reservation['node_seconds']
    assert reserve == world*wall/8
    phase, step = context['budgets']['claude87_stage6'],context['budgets']['step_1']
    assert phase['cap_node_seconds'] == 18000 and step['cap_node_seconds'] == 194400
    available = []
    for budget in (phase,step):
        cap, charged, held = (budget[key] for key in ('cap_node_seconds','charged_node_seconds','reserved_node_seconds'))
        assert all(isfinite(value) and value >= 0 for value in (cap,charged,held))
        assert held >= reserve and charged+held <= cap
        available.append(cap-charged-(held-reserve))
    assert reserve <= min(available)
    return min(available)


class AllocationClock:
    def __init__(self, context, job_id, world, runtime_commit):
        now, monotonic = time.time(),time.monotonic()
        allowance = validate_allocation(context,job_id,world,runtime_commit,now)
        self.world = world
        self.start = monotonic-(now-context['allocation']['start_at'])
        self.allowance = allowance
        self.deadline = min(monotonic+context['allocation']['end_at']-now,
                            self.start+allowance*8/world)-210

    def remaining_node_seconds(self):
        return max(0.,self.allowance-(time.monotonic()-self.start)*self.world/8)
