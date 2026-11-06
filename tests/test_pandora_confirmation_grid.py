"""Pandora's Box after a combat discard-pile grid: the confirmation cards must survive collection."""
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Training fault 5047: a stale Discard grid source let collect_cards release the transformed cards.
RECORD = ROOT / 'docs/faults/episode-fault5047-seed1341088830-after1802.json'
REPLAY = '''
import json, sys, spire
d = json.load(open(sys.argv[1]))
b = spire.TrainingBatch(1, 1, 100000, [d["seed"]], 511)
b.reset_to(0, d["seed"], d["candidates"])
print(int(b.faults[0]))
'''


class PandoraConfirmationGrid(unittest.TestCase):
    def test_recorded_fault_replays_cleanly(self):
        # The host build traps on SPIRE_ASSERT, so replay in a child process.
        result = subprocess.run([sys.executable, '-c', REPLAY, str(RECORD)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.split(), ['0'])
        self.assertEqual(json.loads(RECORD.read_text())['start_kind'], 'fresh')


if __name__ == '__main__':
    unittest.main()
