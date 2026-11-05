"""Corpus selection controls: portable filenames and explicit zero denominators."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import screen


class CorpusSelectionControls(unittest.TestCase):
    def test_nonproject_filename_is_included(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = root / 'candidate'
            candidate.mkdir()
            source = root / 'probe.s'
            source.write_text('.text\n.globl probe\n.type probe,@function\nprobe:\n'
                              'mov 4(%esp),%eax\nmovl $0,4(%eax)\nret\n.size probe,.-probe\n')
            native = root / 'native.o'
            subprocess.run(['as', '--32', '-o', str(native), str(source)], check=True,
                           capture_output=True)
            source.write_text(source.read_text().replace('$0,4(%eax)', '$0,8(%eax)'))
            subprocess.run(['as', '--32', '-o', str(candidate / 'ordinary.o'), str(source)],
                           check=True, capture_output=True)
            with contextlib.redirect_stdout(io.StringIO()):
                screen.run(native, candidate, root / 'report')
            result = json.loads((root / 'report/corpus.json').read_text())
            self.assertEqual(result['objects'], 1)
            self.assertEqual(result['inexact_defining_copies_selected'], 1)
            self.assertEqual(result['functions'][0]['object'], 'ordinary.o')

    def test_empty_candidate_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, 'no candidate object files'):
                screen.run(root / 'missing.o', root, root / 'report')
            self.assertFalse((root / 'report/corpus.json').exists())


if __name__ == '__main__':
    unittest.main()
