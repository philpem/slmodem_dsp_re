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
         'toolchain/induction_jumptable.py', 'toolchain/readonly_copy.py',
         'toolchain/framefree.py', 'toolchain/fidelity.py',
         'toolchain/guarded_switch.py', 'toolchain/callee_effects.py',
         'toolchain/linkedcopies.py', 'ci/period_metrics.py')


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
        self.assertEqual(len(report['files']), 12)
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
        for relative in ('gcc3_owner_trace.py', 'toolchain/framefree.py'):
            with self.subTest(helper=relative):
                result = self.run_child(
                    "import bootstrap;ctx=bootstrap.CONTEXT;p=ctx.tools/" + repr(relative) + ";"
                    "p.chmod(0o600);p.write_bytes(p.read_bytes()+b'\\n# changed');ctx.manifest()")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('captured helper changed: ' + relative, result.stderr)

    def test_conflicting_preloaded_helper_is_rejected(self):
        for name in ('byteident', 'framefree', 'fidelity', 'guarded_switch',
                     'callee_effects', 'linkedcopies', 'period_metrics'):
            with self.subTest(helper=name):
                result = self.run_child(
                    "import types;sys.modules[" + repr(name) + "]=types.ModuleType(" +
                    repr(name) + ");import bootstrap")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('conflicting preloaded helper module: ' + name, result.stderr)

    def test_new_classifier_helpers_keep_frozen_origins(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in FILES:
                dst = root / name
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(TOOLS / name, dst)
            program = '''
import bootstrap, hashlib, json, sys
ctx = bootstrap.CONTEXT
helpers = {
    'framefree': 'toolchain/framefree.py',
    'fidelity': 'toolchain/fidelity.py',
    'guarded_switch': 'toolchain/guarded_switch.py',
    'callee_effects': 'toolchain/callee_effects.py',
    'linkedcopies': 'toolchain/linkedcopies.py',
    'period_metrics': 'ci/period_metrics.py',
}
origins = {}
for name, relative in helpers.items():
    loaded = bootstrap.Path(sys.modules[name].__file__).resolve()
    origins[name] = (loaded == ctx.tools / relative and
                    hashlib.sha256(loaded.read_bytes()).hexdigest() ==
                    ctx.records[relative]['sha256'])
    (ctx.directory / relative).write_bytes(b'broken after capture')
# Exercise the delayed CI import after every original new helper has changed.
digest = sys.modules['byteident'].body_digest(b'\\xc3', {})
ctx.verify()
print(json.dumps({'origins': origins, 'body_digest': digest}))
'''
            result = self.run_child(program, root)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(len(report['origins']), 6)
            self.assertTrue(all(report['origins'].values()), report['origins'])
            self.assertEqual(report['body_digest'],
                             'f21fc00d2eb486d3a668e03a9d17b6a0ac03c05a8c5b208154998504f7ebb6d6')


if __name__ == '__main__':
    unittest.main()
