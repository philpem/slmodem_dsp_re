"""Captured-helper provenance controls, independent of reconstruction semantics."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
FILES = ('gcc3_owner_trace.py', 'gcc3_search_width.py',
         'toolchain/byteident.py', 'toolchain/jumptable.py',
         'toolchain/induction_jumptable.py', 'toolchain/readonly_copy.py')


class BootstrapControls(unittest.TestCase):
    def run_child(self, program, helper_dir=None):
        env = os.environ.copy()
        if helper_dir is None:
            env.pop('NATIVE_EFFECT_DEPENDENCY_TOOLS', None)
        else:
            env['NATIVE_EFFECT_DEPENDENCY_TOOLS'] = str(helper_dir)
        flags = ['-O'] if sys.flags.optimize else []
        return subprocess.run([sys.executable] + flags + ['-c',
            'import sys;sys.path.insert(0,' + repr(str(HERE)) + ');' + program],
            capture_output=True, text=True, env=env, cwd='/tmp')

    def test_default_repository_helpers_without_manifest(self):
        result = self.run_child('import bootstrap,json;print(json.dumps(bootstrap.CONTEXT.manifest()))')
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['source_tools_directory'], str(TOOLS))
        self.assertEqual(len(report['files']), 6)
        self.assertEqual(report['capture'], 'private helper snapshot before import')

    def test_original_change_after_capture_leaves_loaded_bytes_intact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in FILES:
                dst = root / name
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(TOOLS / name, dst)
            result = self.run_child(
                "import bootstrap,hashlib;ctx=bootstrap.CONTEXT;"
                "before=ctx.manifest()['files']['gcc3_owner_trace.py']['sha256'];"
                "source=ctx.directory/'gcc3_owner_trace.py';source.write_bytes(b'broken after capture');"
                "after=ctx.manifest()['files']['gcc3_owner_trace.py']['sha256'];"
                "print(before==after,ctx.owner.__file__.startswith(str(ctx.tools)))", root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), 'True True')

    def test_snapshot_tampering_is_rejected(self):
        result = self.run_child(
            "import bootstrap;ctx=bootstrap.CONTEXT;p=ctx.tools/'gcc3_owner_trace.py';"
            "p.chmod(0o600);p.write_bytes(p.read_bytes()+b'\\n# changed');ctx.manifest()")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('captured helper changed', result.stderr)

    def test_conflicting_preloaded_helper_is_rejected(self):
        result = self.run_child(
            "import types;sys.modules['byteident']=types.ModuleType('byteident');import bootstrap")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('conflicting preloaded helper module: byteident', result.stderr)


if __name__ == '__main__':
    unittest.main()
