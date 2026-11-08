#!/usr/bin/env python3
"""Exercise explicit proof-artifact binding through byteident's real CLI.

The synthetic machine inputs reuse callee_frame_controls' existing caller and
helper. Optional actual inputs check v22_local_loop outside its build tree.
No reconstruction source or stored classification floor is changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import callee_frame_controls as frame

ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / 'tools/toolchain/byteident.py'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--work-dir', type=Path, required=True)
    ap.add_argument('--tc-out', type=Path)
    ap.add_argument('--link-manifest', type=Path)
    ap.add_argument('--linked', type=Path)
    args = ap.parse_args()
    actual = (args.tc_out, args.link_manifest, args.linked)
    if any(actual) and not all(actual):
        ap.error('actual controls need --tc-out, --link-manifest and --linked')
    out = args.work_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / 'RESULTS.json').unlink(missing_ok=True)
    artifacts = out / 'artifacts'
    artifacts.mkdir(exist_ok=True)
    shutil.rmtree(out / 'external', ignore_errors=True)
    external = out / 'external' / 'tc_out'
    external.mkdir(parents=True, exist_ok=True)
    # The report refuses a zero duplicate-name denominator. Supply one
    # identical weak COMDAT solely for that diagnostic, not for callee proof.
    marker = '''\n.section .gnu.linkonce.t.cli_marker,"ax",@progbits
.weak cli_marker
.type cli_marker,@function
cli_marker: ret
.size cli_marker,.-cli_marker
'''
    for name, text in [('caller', frame.CALLER), ('helper', frame.HELPER),
                       ('wrong-helper', frame.HELPER.replace('or $1,%dl', 'or $0,%dl'))]:
        source = artifacts / (name + '.s')
        source.write_text(text + marker)
        subprocess.run(['as', '--32', '-o', str(source.with_suffix('.o')), str(source)],
                       check=True, capture_output=True)
    linked = artifacts / 'linked.o'
    wrong = artifacts / 'wrong-linked.o'
    for target, helper in [(linked, 'helper'), (wrong, 'wrong-helper')]:
        subprocess.run(['ld', '-m', 'elf_i386', '-r', '-o', str(target),
                        str(artifacts / (helper + '.o')), str(artifacts / 'caller.o')],
                       check=True, capture_output=True)
    inputs = artifacts / 'manifest-inputs'
    inputs.mkdir(exist_ok=True)
    for name in ('caller.o', 'helper.o'):
        shutil.copyfile(artifacts / name, inputs / name)
    manifest = inputs / 'manifest.txt'
    manifest.write_text('helper.o helper.s\ncaller.o caller.s\n')
    # Only the unresolved caller is in TC_OUT; the selected helper lives with
    # the explicit manifest, outside TC_OUT's default recovered-link location.
    shutil.copyfile(artifacts / 'caller.o', external / 'caller.o')
    records = []

    def run(label, tc, blob, options=(), symbol='caller', why=False):
        target = out / (label + '.json')
        target.unlink(missing_ok=True)
        command = [sys.executable, str(CLI), *map(str, options)]
        command += ['--why', symbol] if why else ['--limit', '0', '--json-out', str(target)]
        env = dict(os.environ, TC_OUT=str(tc), BLOB=str(blob))
        proc = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
        (out / (label + '.stdout')).write_text(proc.stdout)
        (out / (label + '.stderr')).write_text(proc.stderr)
        data = json.loads(target.read_text()) if target.is_file() and not why else None
        fidelity = (data['function_fidelity'][symbol]['fidelity'] if data else
                    next((line.split('fidelity ', 1)[1].strip() for line in proc.stdout.splitlines()
                          if ': fidelity ' in line), None))
        record = {'label': label, 'exit': proc.returncode, 'fidelity': fidelity,
                  'command': command}
        records.append(record)
        return proc, data, record

    def accepted(result):
        assert result[0].returncode == 0 and result[2]['fidelity'] == 'EXACT', result[2]

    def refused(result):
        assert result[0].returncode != 0 or result[2]['fidelity'] not in ('EXACT', 'REGALLOC'), result[2]

    options = ['--link-manifest', manifest, '--linked', linked]
    # Required primary regression: report selection changes metadata, never
    # the proof context or any function fidelity record.
    plain = run('explicit', external, linked, options)
    reported = run('explicit-report', external, linked, options + ['--link-selected'])
    accepted(plain); accepted(reported)
    assert plain[1]['function_fidelity'] == reported[1]['function_fidelity']
    assert plain[1]['selected_exact'] is None
    assert reported[1]['selected_exact'] is not None
    for report in (False, True):
        accepted(run('why-' + str(report), external, linked,
                     options + (['--link-selected'] if report else []), why=True))
    refused(run('implicit-missing', external, linked))

    # Single explicit options retain the established default for the partner.
    partial = external.parent / 'partial'
    partial.mkdir(exist_ok=True)
    shutil.copyfile(linked, partial / 'dsplibs.o')
    accepted(run('manifest-only', external, linked, ['--link-manifest', manifest]))
    defaults = external.parent / 'tc_repro'
    defaults.mkdir(exist_ok=True)
    for name in ('caller.o', 'helper.o'):
        shutil.copyfile(artifacts / name, defaults / name)
    shutil.copyfile(manifest, defaults / 'tc_link_manifest.txt')
    accepted(run('linked-only', external, linked, ['--linked', linked]))
    implicit = run('implicit-present', external, linked)
    accepted(implicit)
    assert implicit[1]['function_fidelity'] == plain[1]['function_fidelity']

    missing = artifacts / 'missing'
    empty = artifacts / 'empty'
    empty.write_bytes(b'')
    malformed = artifacts / 'malformed'
    malformed.write_text('not an ELF or manifest\n')
    for label, flags in [
        ('missing-link', ['--link-manifest', manifest, '--linked', missing]),
        ('empty-link', ['--link-manifest', manifest, '--linked', empty]),
        ('missing-manifest', ['--link-manifest', missing, '--linked', linked]),
        ('empty-manifest', ['--link-manifest', empty, '--linked', linked]),
        ('empty-link-option', ['--link-manifest', manifest, '--linked', '']),
        ('empty-manifest-option', ['--link-manifest', '', '--linked', linked]),
        ('malformed-link', ['--link-manifest', manifest, '--linked', malformed]),
        ('malformed-manifest', ['--link-manifest', malformed, '--linked', linked]),
        ('wrong-provider', ['--link-manifest', manifest, '--linked', wrong]),
    ]:
        for why in (False, True):
            refused(run(label + ('-why' if why else ''), external, linked, flags, why=why))

    if all(actual):
        tc = out / 'actual-external' / 'tc_out'
        tc.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.tc_out / 'src_pump_v22_v22mod.c.o',
                        tc / 'src_pump_v22_v22mod.c.o')
        blob = ROOT / 'ref/slmodemd/dsplibs.o'
        flags = ['--link-manifest', args.link_manifest.resolve(), '--linked', args.linked.resolve()]
        a = run('actual-explicit', tc, blob, flags, 'v22_local_loop')
        b = run('actual-report', tc, blob, flags + ['--link-selected'], 'v22_local_loop')
        accepted(a); accepted(b)
        assert a[1]['function_fidelity'] == b[1]['function_fidelity']
        for report in (False, True):
            accepted(run('actual-why-' + str(report), tc, blob,
                         flags + (['--link-selected'] if report else []), 'v22_local_loop', True))
        refused(run('actual-implicit-missing', tc, blob, symbol='v22_local_loop'))

    payload = {'controls': len(records), 'failures': 0, 'records': records,
               'assembler': subprocess.check_output(['as', '--version'], text=True).splitlines()[0],
               'scope': 'CLI context binding only; existing proofs/classifier unchanged',
               'cli_sha256': hashlib.sha256(CLI.read_bytes()).hexdigest()}
    (out / 'RESULTS.json').write_text(json.dumps(payload, indent=2) + '\n')
    print('%d CLI controls, 0 failures; actual v22 controls %s' %
          (len(records), 'included' if all(actual) else 'not requested'))


if __name__ == '__main__':
    main()
