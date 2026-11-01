import ast
from copy import deepcopy
from pathlib import Path
import pickle
import runpy
import sys
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from train.e4_checkpoint import load_matching_rank, previous_rank_path, write_rank_checkpoint


def save(value, path):
    Path(path).write_bytes(pickle.dumps(value))


def load(path):
    return pickle.loads(Path(path).read_bytes())


def snapshot(update=426, serial=85, partial=0):
    return dict(update=(update, serial), collector=dict(pools=[[1,2],[3,4]],
        RewardLedger=dict(total=19), rng=b'collector-rng'), ppo_rng=b'ppo-rng',
        torch_rng=b'torch-rng', cursor=dict(optimizer_steps=partial,
        optimizer_attempts=partial,amp_skipped_steps=0) if partial else None)


class CheckpointTests(unittest.TestCase):
    def setUp(self):
        self.directory=TemporaryDirectory(dir=Path(__file__).parent)
        self.addCleanup(self.directory.cleanup)
        self.path=Path(self.directory.name)/'rank-0.pt'
        self.old=snapshot()
        self.new=snapshot(427,86,36)
        save(self.old,self.path)

    def test_legacy_current_and_missing_identity(self):
        self.assertEqual(load_matching_rank(self.path,(426,85),load),(self.old,self.path))
        self.assertEqual(load_matching_rank(self.path,(425,84),load),(None,None))

    def test_new_rank_before_shared_commit_keeps_old_payload(self):
        write_rank_checkpoint(self.path,self.new,save,self.path)
        restored,source=load_matching_rank(self.path,(426,85),load)
        self.assertEqual(restored,self.old)
        self.assertEqual(source,previous_rank_path(self.path))
        self.assertEqual(load(self.path),self.new)

    def test_serialization_interruption_preserves_current(self):
        def interrupted(value,path):
            Path(path).write_bytes(b'incomplete')
            raise InterruptedError()
        with self.assertRaises(InterruptedError):
            write_rank_checkpoint(self.path,self.new,interrupted,self.path)
        self.assertEqual(load_matching_rank(self.path,(426,85),load),(self.old,self.path))

    def test_interruption_before_rank_replace_preserves_committed_generation(self):
        replace=Path.replace
        def interrupted(source,target):
            if source==Path(str(self.path)+'.tmp'):
                raise InterruptedError()
            return replace(source,target)
        with patch.object(Path,'replace',interrupted),self.assertRaises(InterruptedError):
            write_rank_checkpoint(self.path,self.new,save,self.path)
        self.assertEqual(load_matching_rank(self.path,(426,85),load)[0],self.old)
        self.assertEqual(load(previous_rank_path(self.path)),self.old)

    def test_repeated_interrupted_attempt_does_not_rotate_uncommitted_rank(self):
        write_rank_checkpoint(self.path,self.new,save,self.path)
        committed=previous_rank_path(self.path)
        expected=committed.read_bytes()
        for update in (428,429):
            write_rank_checkpoint(self.path,snapshot(update,86,52),save,committed)
            self.assertEqual(committed.read_bytes(),expected)
            self.assertEqual(load_matching_rank(self.path,(426,85),load)[0],self.old)

    def test_shared_commit_advances_next_retained_generation(self):
        write_rank_checkpoint(self.path,self.new,save,self.path)
        self.assertEqual(load_matching_rank(self.path,(427,86),load),(self.new,self.path))
        write_rank_checkpoint(self.path,snapshot(430,87),save,self.path)
        self.assertEqual(load_matching_rank(self.path,(427,86),load)[0],self.new)

    def test_eight_ranks_survive_a_partially_written_generation(self):
        paths=[self.path.parent/f'rank-{rank}.pt' for rank in range(8)]
        expected=[]
        for rank,path in enumerate(paths):
            value=deepcopy(self.old);value['collector']['rng']=bytes([rank])
            save(value,path);expected.append(value)
        for rank in (0,7,3):
            write_rank_checkpoint(paths[rank],self.new,save,paths[rank])
        self.assertEqual([load_matching_rank(path,(426,85),load)[0] for path in paths],expected)

    def test_previous_readable_when_current_absent(self):
        self.path.replace(previous_rank_path(self.path))
        self.assertEqual(load_matching_rank(self.path,(426,85),load)[0],self.old)

    def test_controller_partial_matches_shared_identity(self):
        repo=Path(__file__).parents[1]
        training=self.path.parent/'training';training.mkdir()
        common=dict(state=dict(update=426,checkpoint_serial=85),
                    optimizer=dict(state={0:dict(step=27254)}))
        save(common,training/'model.pt')
        save(self.new,training/'rank-0.pt')
        old=deepcopy(self.old)
        old['cursor']=dict(optimizer_steps=12,optimizer_attempts=13,amp_skipped_steps=1)
        save(old,previous_rank_path(training/'rank-0.pt'))
        module=ast.parse((repo/'tools/e4_stage12_chain.py').read_text())
        function=next(n for n in module.body if isinstance(n,ast.FunctionDef) and n.name=='checkpoint_state')
        namespace=dict(Path=Path,runpy=runpy)
        exec(compile(ast.Module(body=[function],type_ignores=[]),'<checkpoint_state>','exec'),namespace)
        torch=SimpleNamespace(load=lambda path,**_:load(path))
        with patch.dict(sys.modules,torch=torch):
            result=namespace['checkpoint_state'](dict(training_root=str(self.path.parent),repository=str(repo)))
        self.assertEqual((result['optimizer_steps'],result['partial_steps'],
                          result['partial_attempts'],result['partial_skips']),(27254,12,13,1))
        self.assertTrue(result['rank_checkpoint_match'])
        self.assertTrue(result['rank_checkpoint_path'].endswith('.previous'))


if __name__=='__main__':
    unittest.main()
