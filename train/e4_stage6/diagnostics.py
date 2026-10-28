def b_round_diagnostics(rows, round_number, start, end, entropy_target):
    selected = [r for r in rows if start < r['training_node_seconds'] <= end]
    def mean(key, weight):
        total = sum(r[weight] for r in selected)
        return sum(r[key] * r[weight] for r in selected) / total if total else None
    moments = [r.get('copy_advantage_moments_global') for r in selected]
    complete = bool(moments) and all(m is not None for m in moments)
    combined = [sum(m[i] for m in moments) for i in range(3)] if complete else None
    std = (max(0., combined[1] / combined[2] - (combined[0] / combined[2])**2)**.5
           if complete and combined[2] else None)
    fields = ('update','training_node_seconds','H_imit','beta_used','copy_kl',
              'copy_clip_fraction','combat_value_loss')
    return dict(round=round_number, start_node_seconds=start, end_node_seconds=end,
        H_imit=mean('H_imit','imitation_entropy_samples'), entropy_target=entropy_target,
        beta_used=selected[-1]['beta_used'] if selected else None,
        copy_kl=mean('copy_kl','copy_samples'),
        copy_clip_fraction=mean('copy_clip_fraction','copy_samples'),
        combat_value_loss=mean('combat_value_loss','copy_samples'),
        copy_advantage_std_global=std, copy_advantage_moments_global=combined,
        copy_advantage_moments_complete=complete,
        updates=[dict({key:r[key] for key in fields},
            copy_advantage_std_global=r.get('copy_advantage_std_global')) for r in selected],
        scope='Completed updates in this round; sample-weighted metrics, last beta; raw copy population std before scaling, null if unavailable in legacy cursor')


def b_diagnostic_stop(reports):
    last = reports[-1]
    if last['H_imit'] is not None and last['H_imit'] < last['entropy_target'] / 2:
        return 'B_entropy_below_half_target'
    if len(reports) >= 2:
        previous = reports[-2]
        if (last['round'] == previous['round'] + 1 and
            all(r['copy_kl'] is not None and r['copy_kl'] > .05 for r in (previous,last))):
            return 'B_copy_kl_above_0_05_two_rounds'
    return None
