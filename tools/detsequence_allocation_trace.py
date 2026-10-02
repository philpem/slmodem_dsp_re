#!/usr/bin/env python3
"""Trace allocation/reload boundaries on two saved, unchanged Gentoo controls."""
import playbook_small_patterns as driver
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path
import experiment_toolchain as tc


GDB_SCRIPT = r'''
set pagination off
set confirm off
set breakpoint pending on
set follow-fork-mode child
set detach-on-fork on
set disable-randomization off
set startup-with-shell off
python
import gdb, json
def target():
    return gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string() == 'DetSequence'
def snapshot(stage):
    count = int(gdb.parse_and_eval('max_regno'))
    mapping = gdb.parse_and_eval('reg_renumber')
    print('ALLOC_TRACE ' + json.dumps({'stage': stage, 'function': 'DetSequence',
          'mapping': {str(n): int(mapping[n]) for n in range(58, count)}}))
class ReloadReturn(gdb.FinishBreakpoint):
    def stop(self):
        snapshot('reload-return')
        return False
class AllocationEntry(gdb.Breakpoint):
    def stop(self):
        if target(): snapshot('global-entry')
        return False
class ReloadEntry(gdb.Breakpoint):
    def stop(self):
        if target():
            snapshot('reload-entry')
            ReloadReturn(gdb.newest_frame(), internal=True)
        return False
AllocationEntry('global_alloc', internal=True)
ReloadEntry('reload', internal=True)
end
run
'''

FAMILIES = {
    'playbook-detsequence': ('baseline', 'shift-counter'),
    'playbook-detsequence-shift-lifetime': ('baseline', 'separate-decrement'),
}


def output_directory(root, family):
    suffix = '' if family == 'playbook-detsequence' else '-shift-lifetime'
    return root / ('build/detsequence-allocation-trace' + suffix)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def analyze(out, domain):
    result = json.loads((out / 'results.json').read_text())
    assert result['domain'] == domain, 'declared domain drift'
    family = result.get('prior_family', 'playbook-detsequence')
    assert set(result['cells']) == set(FAMILIES[family]), 'incomplete domain'
    for label, row in result['cells'].items():
        directory = out / 'V32prc' / label
        assert (directory / 'compiler.exit').read_text().strip() == '0'
        assert (directory / 'compiler-driver.sh').read_text() == '#!/bin/sh\n' + row['compiler_wrapper'] + '\n'
        assert digest(out / 'V32prc' / label / 'candidate.o') == row['expected_object_hash']
        assert digest(out / 'V32prc' / label / 'V32prc.c') == row['source_hash']
        events = [json.loads(line[len('ALLOC_TRACE '):])
                  for line in (out / 'V32prc' / label / 'trace.log').read_text().splitlines()
                  if line.startswith('ALLOC_TRACE ')]
        assert events == row['events'], 'trace artifact drift'
        assert [e['stage'] for e in events] == ['global-entry', 'reload-entry', 'reload-return']
        computed = (family == 'playbook-detsequence' and label == 'baseline')
        computed = computed or label == 'separate-decrement'
        expected = [(-1, -1), (6, 2), (6, -1)] if computed else [(-1, -1), (-1, 3), (-1, 3)]
        assert [(e['mapping']['64'], e['mapping']['66']) for e in events] == expected
        if label == 'shift-counter':
            assert all(e['mapping']['81'] == 2 for e in events), 'missing local ECX allocation'
    print('raw full-TU controls: 2 / 2; allocation/reload snapshots: 6 / 6')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--domain', required=True)
    ap.add_argument('--analysis-only', action='store_true')
    ap.add_argument('--source-family', choices=tuple(FAMILIES), default='playbook-detsequence')
    args = ap.parse_args()
    root = driver.ROOT
    out = output_directory(root, args.source_family)
    if args.analysis_only:
        analyze(out, args.domain)
        return
    prior = root / 'build' / args.source_family
    saved = json.loads((prior / 'results.json').read_text())
    assert (root / 'build/tc_out/.build-config').read_text() == saved['config']
    for path, expected in saved['headers'].items():
        assert digest(root / path) == expected, path
    image = saved['config'].splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in saved['config'].splitlines()
                            if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/' + f
             if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    out.mkdir(exist_ok=True)
    (out / 'trace.gdb').write_text(GDB_SCRIPT)
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    debugger = ('/host/lib64/ld-linux-x86-64.so.2 --library-path '
                '/host/lib/x86_64-linux-gnu:/host/usr/lib/x86_64-linux-gnu '
                '/host/usr/bin/gdb --data-directory=/host/usr/share/gdb '
                '--nx --batch -x /work/trace.gdb --args /bin/sh ')
    result = {'domain': args.domain, 'prior_domain': saved['domain'],
              'prior_family': args.source_family,
              'config': saved['config'], 'headers': saved['headers'], 'cells': {}}
    for label in FAMILIES[args.source_family]:
        previous = prior / 'V32prc' / label
        cell = saved['families']['V32prc']['cells'][label]
        assert digest(previous / 'candidate.o') == cell['object_hash']
        assert digest(previous / 'V32prc.c') == cell['source_hash']
        destination = out / 'V32prc' / label
        destination.mkdir(parents=True, exist_ok=True)
        for p in previous.glob('*.h'):
            (destination / p.name).write_bytes(p.read_bytes())
        (destination / 'V32prc.c').write_bytes((previous / 'V32prc.c').read_bytes())
        inside = '/work/V32prc/' + label
        compile_command = tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ['-da'],
                                           inside + '/candidate.o', inside + '/V32prc.c')
        assert compile_command.count('exec gcc ') == 1
        # GDB follows cc1, detaching its parents. A wrapper reports the driver's
        # exit after the assembler completes; PID 1 waits before ending Docker.
        status = inside + '/compiler.exit'
        (destination / 'compiler.exit').unlink(missing_ok=True)
        wrapper = (compile_command.replace('exec gcc ', 'gcc ') +
                   '; trace_status=$?; printf "%s\\n" "$trace_status" > ' +
                   shlex.quote(status) + '; exit "$trace_status"')
        (destination / 'compiler-driver.sh').write_text('#!/bin/sh\n' + wrapper + '\n')
        shell = ('cd ' + shlex.quote(inside) + ' && export PYTHONHOME=/host/usr; '
                 + debugger + inside + '/compiler-driver.sh; debugger_status=$?; '
                 'trace_wait=0; while [ ! -f ' + shlex.quote(status) + ' ]; do '
                 'trace_wait=$((trace_wait + 1)); [ "$trace_wait" -le 100 ] || exit 124; '
                 'sleep 0.1; done; [ "$debugger_status" -eq 0 ] || exit "$debugger_status"; '
                 'test "$(cat ' + shlex.quote(status) + ')" = 0')
        command = tc.docker_prefix(image, root, out, True)
        command[2:2] = ['--cap-add=SYS_PTRACE', '--security-opt=seccomp=unconfined',
                        '-v', '/usr:/host/usr:ro', '-v', '/lib:/host/lib:ro',
                        '-v', '/lib64:/host/lib64:ro']
        command += ['/bin/sh', '-c', shell]
        row = {'command': command, 'source_hash': cell['source_hash'],
               'compiler_wrapper': wrapper,
               'expected_object_hash': cell['object_hash']}
        result['cells'][label] = row
        with (destination / 'trace.log').open('w') as log:
            row['exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        assert row['exit'] == 0, label
        row['compiler_exit'] = int((destination / 'compiler.exit').read_text())
        assert row['compiler_exit'] == 0, 'compiler driver or assembler failed'
        row['object_hash'] = digest(destination / 'candidate.o')
        assert row['object_hash'] == cell['object_hash'], 'raw full-TU control drift'
        row['events'] = [json.loads(line[len('ALLOC_TRACE '):])
                         for line in (destination / 'trace.log').read_text().splitlines()
                         if line.startswith('ALLOC_TRACE ')]
        assert [event['stage'] for event in row['events']] == [
            'global-entry', 'reload-entry', 'reload-return'], 'missing or repeated target events'
        for event in row['events']:
            print(label, event['stage'], 'nbits', event['mapping']['64'],
                  'reg', event['mapping']['66'])
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    for path, expected in saved['headers'].items():
        assert digest(root / path) == expected, path
    analyze(out, args.domain)


if __name__ == '__main__':
    main()
