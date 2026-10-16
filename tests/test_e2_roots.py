import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('e2_roots', Path(__file__).resolve().parents[1] / 'tools/e2_roots.py')
e2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e2)


class RootInventoryTests(unittest.TestCase):
    def test_partition_boundary_rejects_locked_without_running_engine(self):
        for name in ('train', 'dev'):
            for low, high in e2.PARTITIONS[name]:
                self.assertEqual(e2.partition(low), name)
                self.assertEqual(e2.partition(high), name)
        for low, high in e2.PARTITIONS['locked_test']:
            for seed in (low, high):
                with self.assertRaises(AssertionError):
                    e2.partition(seed)

    def test_duplicates_cannot_satisfy_family_cap(self):
        template = dict(root=dict(seed=0, candidates=[0]), hp=1, deck=[], relics=[], potions=[],
                        difficulty=dict(uniform=dict(success=0, cutoff=0), search=dict(success=0, cutoff=0)))
        group = []
        for seed in range(20):
            row = copy.deepcopy(template)
            row['root']['seed'] = 1_000_000_000 + seed
            group.append(row)
        self.assertTrue(e2.stage_summary(group)['family_cap_satisfied'])
        self.assertFalse(e2.stage_summary(group[:-1])['family_cap_satisfied'])
        self.assertFalse(e2.stage_summary(group + [group[0]])['family_cap_satisfied'])
        self.assertFalse(e2.stage_summary([])['family_cap_satisfied'])

    def test_compact_gzip_preserves_prefix_and_has_stable_checksum(self):
        root = dict(root=dict(seed=1_999_999_999, candidates=[0, 12, 1, 127]), stage='act2_entry')
        with tempfile.TemporaryDirectory() as directory:
            a, b = (Path(directory) / name for name in ('a.gz', 'b.gz'))
            e2.write_gzip(a, [root])
            e2.write_gzip(b, [root])
            self.assertEqual(list(e2.rows(a)), [root])
            self.assertEqual(e2.digest(a), e2.digest(b))


if __name__ == '__main__':
    unittest.main()
