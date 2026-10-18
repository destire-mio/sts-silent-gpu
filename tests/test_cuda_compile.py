from contextlib import redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest

from tools.cuda_compile import report


class CompileTiming(unittest.TestCase):
    def test_gnu_time_table(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'unit.cu.o.time').write_text(
                '\tElapsed (wall clock) time (h:mm:ss or m:ss): 1:02.50\n'
                '\tMaximum resident set size (kbytes): 123456\n'
                '\tExit status: 0\n')
            output = io.StringIO()
            with redirect_stdout(output): report(path)
            self.assertIn('| unit.cu.o.time | 62.50 | 123456 | 0 |', output.getvalue())


if __name__ == '__main__':
    unittest.main()
