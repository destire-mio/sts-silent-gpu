"""Keep the committed rank generation readable across an interrupted save."""
import os
import shutil
from pathlib import Path


def previous_rank_path(path):
    return Path(str(path) + ".previous")


def load_matching_rank(path, identity, load):
    identity = tuple(identity)
    for candidate_path in (Path(path), previous_rank_path(path)):
        if not candidate_path.exists():
            continue
        candidate = load(candidate_path)
        if tuple(candidate["update"]) == identity:
            return candidate, candidate_path
    return None, None


def write_rank_checkpoint(path, state, save, committed_path=None, *, retain_by_copy=False):
    path = Path(path)
    temporary = Path(str(path) + ".tmp")
    previous = previous_rank_path(path)
    save(state, temporary)
    if committed_path is not None:
        committed_path = Path(committed_path)
        assert committed_path in (path, previous)
        if committed_path != previous:
            retained = Path(str(previous) + ".tmp")
            retained.unlink(missing_ok=True)
            if retain_by_copy:
                shutil.copyfile(committed_path, retained)
            else:
                os.link(committed_path, retained)
            retained.replace(previous)
    temporary.replace(path)
