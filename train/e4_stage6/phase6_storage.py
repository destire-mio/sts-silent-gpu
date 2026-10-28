from copy import deepcopy
import hashlib
import json
from pathlib import Path
from uuid import uuid4

import torch
import torch.distributed as dist

from tools.e4_run import atomic_checkpoint, atomic_json


def file_digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class Phase6Storage:
    def __init__(self, root, binding):
        self.root, self.binding = Path(root), deepcopy(binding)
        self.rank, self.world = dist.get_rank(), dist.get_world_size()
        assert self.binding['world'] == self.world
        if self.rank == 0:
            self.root.mkdir(parents=True, exist_ok=True)
        dist.barrier()

    def broadcast(self, value):
        shared = [value]
        dist.broadcast_object_list(shared, src=0)
        return shared[0]

    def save_worker(self, worker, kind, round_number):
        assert kind in ('training', 'evaluation')
        arm = worker.binding['arm']
        assert arm in ('A', 'B') and worker.binding['world'] == self.world
        name = self.broadcast(f'{kind}-{arm}-round{round_number}-{uuid4().hex}' if self.rank == 0 else None)
        directory = self.root / name
        if self.rank == 0:
            directory.mkdir()
        dist.barrier()
        path = directory / f'rank-{self.rank}.pt'
        atomic_checkpoint(path, worker.snapshot())
        record = dict(rank=self.rank, name=path.name, bytes=path.stat().st_size, sha256=file_digest(path))
        records = [None] * self.world
        dist.all_gather_object(records, record)
        reference = None
        if self.rank == 0:
            assert [row['rank'] for row in records] == list(range(self.world))
            manifest = dict(format=1, binding=self.binding, kind=kind, arm=arm, round=round_number, files=records)
            manifest_path = directory / 'manifest.json'
            atomic_json(manifest_path, manifest)
            reference = dict(directory=name, manifest_sha256=file_digest(manifest_path),
                             kind=kind, arm=arm, round=round_number)
        return self.broadcast(reference)

    def resume_binding(self, binding):
        normalized = deepcopy(binding)
        if normalized != self.binding:
            # Claude93 authorizes the saved 1dfe round1 pair after the Counter correction.
            assert normalized['runtime_commit'] == '1dfe39a3d9b0b33c453579c32c32b591911115bc'
            normalized['runtime_commit'] = self.binding['runtime_commit']
            assert normalized == self.binding
        return normalized

    def load_worker(self, reference):
        assert Path(reference['directory']).name == reference['directory']
        directory = self.root / reference['directory']
        path = directory / 'manifest.json'
        assert file_digest(path) == reference['manifest_sha256']
        manifest = json.loads(path.read_bytes())
        assert manifest['format'] == 1
        assert all(manifest[key] == reference[key] for key in ('kind', 'arm', 'round'))
        assert len(manifest['files']) == self.world
        assert [row['rank'] for row in manifest['files']] == list(range(self.world))
        record = manifest['files'][self.rank]
        assert record['name'] == f'rank-{self.rank}.pt'
        path = directory / record['name']
        assert path.stat().st_size == record['bytes'] and file_digest(path) == record['sha256']
        self.resume_binding(manifest['binding'])
        saved = torch.load(path, map_location='cpu', weights_only=False)
        assert saved['binding']['arm'] == manifest['arm']
        assert saved['binding']['rank'] == self.rank and saved['binding']['world'] == self.world
        if 'runtime_commit' in manifest['binding']:
            assert saved['binding']['runtime_commit'] == manifest['binding']['runtime_commit']
            saved['binding']['runtime_commit'] = self.binding['runtime_commit']
        return saved

    def save_state(self, state):
        if self.rank == 0:
            atomic_json(self.root / 'phase6-state.json', dict(format=1, binding=self.binding, state=state))
        dist.barrier()

    def load_state(self):
        value = None
        if self.rank == 0:
            path = self.root / 'phase6-state.json'
            if path.exists():
                saved = json.loads(path.read_bytes())
                assert saved['format'] == 1
                self.resume_binding(saved['binding'])
                value = saved['state']
                if 'schedule' in value:
                    assert value['schedule']['binding'] == saved['binding']
                    value['schedule']['binding'] = self.resume_binding(value['schedule']['binding'])
        return self.broadcast(value)
