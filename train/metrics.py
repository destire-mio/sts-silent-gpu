import numpy as np

from .model import SCREENS


def calibration(predictions, outcomes):
    if not len(predictions):
        return dict(count=0, brier=None, predicted=None, observed=None, bins=[])
    bins = []
    groups = np.minimum((predictions * 10).astype(int), 9)
    for group in np.unique(groups):
        selected = groups == group
        bins.append(dict(bin=int(group), count=int(selected.sum()), predicted=float(predictions[selected].mean()),
                         observed=float(outcomes[selected].mean())))
    return dict(count=len(predictions), brier=float(np.square(predictions - outcomes).mean()),
                predicted=float(predictions.mean()), observed=float(outcomes.mean()), bins=bins)


def summarize(rollout):
    n = rollout.count
    episodes = rollout.episodes
    completed = [e for e in episodes if e["outcome"] in ("victory", "death")]
    wins = [e for e in episodes if e["outcome"] == "victory"]
    valid = rollout.valid[:n]
    complete = rollout.complete[:n]
    starts = np.array([e["records"][0] for e in completed], dtype=np.int32)
    entropy = {}
    by_screen = {}
    for screen in np.unique(rollout.screens[:n]):
        selected = rollout.screens[:n] == screen
        entropy[SCREENS[screen]] = dict(decisions=int(selected.sum()), normalized=float(rollout.entropy[:n][selected].mean()))
        terminal = selected & complete
        by_screen[SCREENS[screen]] = calibration(rollout.values[:n][terminal], rollout.returns[:n][terminal])
    by_act = {}
    for act in np.unique(rollout.acts[:n]):
        selected = (rollout.acts[:n] == act) & complete
        by_act[str(act)] = calibration(rollout.values[:n][selected], rollout.returns[:n][selected])
    return dict(episodes=len(episodes), completed=len(completed), wins=len(wins),
                root_families=len({e["seed"] for e in episodes}),
                root_families_won=len({e["seed"] for e in wins}),
                natural_starts=sum(e["prefix_decisions"] == 0 for e in episodes),
                completed_natural_starts=sum(e["prefix_decisions"] == 0 for e in completed),
                truncated=sum(e["outcome"] == "truncated" for e in episodes),
                faults=sum(e["outcome"] == "fault" for e in episodes),
                decisions=n, training_decisions=int(valid.sum()),
                replay_decisions=sum(e["prefix_decisions"] for e in episodes),
                compact_bytes=int(rollout.offsets[n]) + (n + 1) * 8,
                per_screen_entropy=entropy,
                value_calibration=dict(episode_start=calibration(rollout.values[starts], rollout.returns[starts]),
                                       decision_weighted=calibration(rollout.values[:n][complete], rollout.returns[:n][complete]),
                                       by_screen=by_screen, by_act=by_act))
