from collections import defaultdict
from math import sqrt
from statistics import mean, stdev


HARD_GROUPS = {'act1_elite', 'act1_boss_guardian', 'act1_boss_hexaghost',
               'act1_boss_slimeboss', 'act2_elite_boss'}


def key(row):
    return row['seed'], tuple(row['prefix']), row['mode'], row['repetition']


def summary(rows):
    groups = defaultdict(list)
    for row in rows:
        groups[row['group'], row['mode']].append(row)
    return {group + '/' + mode: dict(samples=len(values), death_rate=mean(float(r['death']) for r in values),
                mean_score=mean(r['score'] for r in values)) for (group, mode), values in groups.items()}


def cluster_standard_error(rows):
    clusters = defaultdict(list)
    for seed, difference in rows:
        clusters[seed].append(difference)
    if len(clusters) < 2:
        return None
    count = sum(map(len, clusters.values()))
    delta = sum(sum(values) for values in clusters.values()) / count
    squared = sum((sum(values) - delta * len(values))**2 for values in clusters.values())
    return sqrt(len(clusters) / (len(clusters) - 1) * squared) / count


def compare(arm_a, arm_b, episodes_a, episodes_b, baseline):
    a, b = {key(row): row for row in arm_a}, {key(row): row for row in arm_b}
    assert len(a) == len(arm_a) and len(b) == len(arm_b) and set(a) == set(b)
    for identity in a:
        assert a[identity]['group'] == b[identity]['group']
    scenes = {(r['seed'], tuple(r['prefix'])) for r in arm_a}
    for seed, prefix in scenes:
        assert {(k[2], k[3]) for k in a if k[:2] == (seed, prefix)} == {('replay32', 0), ('single', 0), ('single', 1), ('single', 2), ('single', 3)}
    hard = [(r['seed'], float(r['death']) - float(b[identity]['death'])) for identity, r in a.items()
            if r['mode'] == 'replay32' and r['group'] in HARD_GROUPS]
    assert hard
    hard_delta = mean(d for _, d in hard)
    hard_se = cluster_standard_error(hard)
    full = [{e['seed']: e['floor'] for e in rows} for rows in (episodes_a, episodes_b, baseline)]
    assert all(len(rows) == len(mapping) == 1024 and set(mapping) == set(range(2000000000, 2000001024))
               for rows, mapping in zip((episodes_a, episodes_b, baseline), full))
    paired = [full[1][seed] - full[0][seed] for seed in sorted(full[0])]
    floor_delta, floor_se = mean(paired), stdev(paired) / sqrt(1024)
    baseline_comparison = {}
    for name, mapping in zip(('A', 'B'), full[:2]):
        differences = [mapping[seed] - full[2][seed] for seed in sorted(mapping)]
        baseline_comparison[name] = dict(mean_floor=mean(mapping.values()),
            paired_delta_from_B4145=mean(differences), paired_SE=stdev(differences) / sqrt(1024))
    hard_pass = hard_se is not None and hard_delta > 3 * hard_se
    floor_pass = floor_delta >= -floor_se
    return dict(A=summary(arm_a), B=summary(arm_b), hard_replay32_death_improvement_A_minus_B=hard_delta,
        hard_replay32_SE_clustered_by_source_seed=hard_se, hard_replay32_samples=len(hard),
        hard_source_seeds=len({s for s, _ in hard}), hard_death_test_pass=hard_pass,
        final_floor_delta_B_minus_A=floor_delta, final_floor_paired_SE=floor_se,
        final_floor_test_pass=floor_pass, passed=hard_pass and floor_pass,
        against_original_B4145=baseline_comparison,
        statistical_scope='Matched seed+prefix and mode/repetition. Hard replay32 excludes Act2 normals; SE clusters all combat entries from the same original seed. Single-play x4 draws reported as draws, not four independent source runs. Final1024 SE uses perseed paired floor deltas.')
