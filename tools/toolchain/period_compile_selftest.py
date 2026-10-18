#!/usr/bin/env python3
"""Exercise real period shell dispatch/cache with recording compiler stand-ins."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ('harness', 'runtime', 'fakedp', 'v34hsstep', 'unwritten')
INPUTS = ('src/core/plain.c', 'src/service/dcr.c', 'test/unit/fixture.c',
          'src/dsp/plain.cpp', 'test/unit/cppfixture.cpp', 'src/core/asm_input.S') + tuple(
              'test/harness/'+name+'.c' for name in HARNESS)


class CompileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='period-compile-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root/'compiler.jsonl'
        self.out = self.root/'out'
        self.script = self.root/'tools/toolchain/period_inner.sh'
        self.script.parent.mkdir(parents=True)
        source = Path(os.environ.get('PERIOD_COMPILE_TEST_SCRIPT', ROOT/'tools/toolchain/period_inner.sh')).read_text()
        # The container's fixed /out mount is the sole adapted shell line.
        self.assertEqual(source.count('OUT=/out\n'), 1)
        self.script.write_text(source.replace('OUT=/out\n', 'OUT='+shlex.quote(str(self.out))+'\n'))
        for name in INPUTS + ('include/test.h', 'test/unit/cppfixture.cxxflags'):
            path = self.root/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('-fno-inline\n' if name.endswith('.cxxflags') else '/* input */\n')
            os.utime(path, (1_700_000_000, 1_700_000_000))
        os.utime(self.script, (1_700_000_000, 1_700_000_000))
        bindir = self.root/'bin'
        bindir.mkdir()
        compiler = bindir/'gcc'
        compiler.write_text('#!'+sys.executable+'\n'+'''import json, os, pathlib, sys
args = sys.argv[1:]
if args == ['-dumpversion']:
    print('3.4.2')
    sys.exit(0)
assert '-c' in args, args
with open(os.environ['COMPILER_LOG'], 'a') as stream:
    stream.write(json.dumps({'compiler': pathlib.Path(sys.argv[0]).name, 'argv': args})+'\\n')
output = pathlib.Path(args[args.index('-o')+1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(b'compiled')
''')
        compiler.chmod(0o755)
        (bindir/'g++').symlink_to('gcc')
        self.env = dict(os.environ, PATH=str(bindir)+os.pathsep+os.environ.get('PATH', ''),
                        COMPILER_LOG=str(self.log), KEEP='1', STAGE='compile', J='1',
                        SRC=' '.join(INPUTS[:3]), CXXSRC=' '.join(INPUTS[3:5]), ASRC=INPUTS[5], TESTS='')

    def run_stage(self):
        result = subprocess.run(['sh', str(self.script)], cwd=self.root, env=self.env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)
        return result.stdout

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def by_source(self):
        self.run_stage()
        return {call['argv'][-1]: call['argv'] for call in self.calls()}

    def test_source_c_keeps_default_dialect(self):
        argv = self.by_source()['src/core/plain.c']
        self.assertFalse(any(flag.startswith('-std=') for flag in argv), argv)
        self.assertIn('-DDSPLIB_REPRODUCE_BUGS', argv)

    def test_fixture_and_harness_c_keep_gnu99(self):
        calls = self.by_source()
        for source in ('test/unit/fixture.c',) + INPUTS[6:]:
            self.assertIn('-std=gnu99', calls[source], source)

    def test_dcr_flags_stay_local_and_keep_default_dialect(self):
        calls = self.by_source()
        dcr = calls['src/service/dcr.c']
        self.assertFalse(any(flag.startswith('-std=') for flag in dcr), dcr)
        self.assertLess(dcr.index('-O3'), dcr.index('-O2'))
        self.assertIn('-fno-rerun-cse-after-loop', dcr)
        for source, argv in calls.items():
            if source != 'src/service/dcr.c':
                self.assertNotIn('-fno-rerun-cse-after-loop', argv, source)

    def test_cxx_and_assembly_dispatch_unchanged(self):
        calls = self.by_source()
        self.assertIn('-ffast-math', calls['src/dsp/plain.cpp'])
        self.assertNotIn('-ffast-math', calls['test/unit/cppfixture.cpp'])
        self.assertIn('-fno-inline', calls['test/unit/cppfixture.cpp'])
        self.assertNotIn('-fno-inline', calls['src/dsp/plain.cpp'])
        self.assertFalse(any(flag.startswith('-std=') for flag in calls['asm_input.S']))
        self.assertNotIn('-include', calls['asm_input.S'])
        self.assertEqual(sum(call['compiler'] == 'g++' for call in self.calls()), 2)

    def test_printed_c_flags_match_actual_argv(self):
        stdout = self.run_stage()
        calls = {call['argv'][-1]: call['argv'] for call in self.calls()}
        for label, source in [('C source', 'src/core/plain.c'), ('C fixture', 'test/unit/fixture.c'),
                              ('DCR source', 'src/service/dcr.c')]:
            prefix = 'period: '+label+' flags '
            lines = [line for line in stdout.splitlines() if line.startswith(prefix)]
            self.assertEqual(len(lines), 1, stdout)
            self.assertEqual(shlex.split(lines[0][len(prefix):]), calls[source][1:-3])

    def test_keep_reuses_unchanged_objects(self):
        self.run_stage()
        count = len(self.calls())
        self.assertEqual(count, len(INPUTS))
        self.run_stage()
        self.assertEqual(len(self.calls()), count)

    def newer_than_objects(self, path):
        newest = max(p.stat().st_mtime_ns for p in self.out.glob('*.o')) + 1
        os.utime(path, ns=(newest, newest))

    def test_apparatus_script_edit_rebuilds_cached_objects(self):
        self.run_stage()
        self.run_stage()
        self.assertEqual(len(self.calls()), len(INPUTS))
        self.script.write_text(self.script.read_text()+'\n# apparatus revision\n')
        self.newer_than_objects(self.script)
        self.run_stage()
        self.assertEqual(len(self.calls()), 2*len(INPUTS))

    def test_header_edit_rebuilds_cached_objects(self):
        self.run_stage()
        self.newer_than_objects(self.root/'include/test.h')
        self.run_stage()
        self.assertEqual(len(self.calls()), 2*len(INPUTS))

    def test_inc_only_edit_rebuilds_cached_objects(self):
        # Runtime conservatively invalidates every object on any included-body
        # edit. Exercise all three scanned trees without changing .c/.h inputs.
        watched = [self.root/name for name in INPUTS + ('include/test.h',)]
        watched.append(self.script)
        original_times = [path.stat().st_mtime_ns for path in watched]
        for directory in ('include', 'src/core', 'test/harness'):
            with self.subTest(directory=directory):
                inc = self.root/directory/'body.inc'
                inc.write_text('/* initial extracted body */\n')
                os.utime(inc, (1_700_000_000, 1_700_000_000))
                self.run_stage()
                count = len(self.calls())
                self.run_stage()
                self.assertEqual(len(self.calls()), count)
                inc.write_text('/* changed extracted body only */\n')
                self.newer_than_objects(inc)
                self.run_stage()
                self.assertEqual(len(self.calls()), count + len(INPUTS))
                self.assertEqual([path.stat().st_mtime_ns for path in watched],
                                 original_times)

    def test_source_edit_rebuilds_only_affected_object(self):
        self.run_stage()
        self.newer_than_objects(self.root/'src/core/plain.c')
        self.run_stage()
        self.assertEqual(len(self.calls()), len(INPUTS)+1)
        self.assertEqual(self.calls()[-1]['argv'][-1], 'src/core/plain.c')

    def test_empty_keep_rebuilds_objects(self):
        self.run_stage()
        self.env['KEEP'] = ''
        self.run_stage()
        self.assertEqual(len(self.calls()), 2*len(INPUTS))


if __name__ == '__main__':
    unittest.main(verbosity=2)
