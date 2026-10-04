#!/usr/bin/env python3
"""FloatFIR/FloatIIR ctor family: the 7-byte je-target alignment nop and the
arg2/arg3 register swap, as two separable value-identical spellings.

Posted as the "FloatFIR/FloatIIR ctor family" comment in #22.  Cells:

  baseline                     retained objects, raw-checked
  hist                         malloc result through a local (nop lever)
  len                          bufferLength/m_len assigned before coefficients
  hist+len                     candidate: both levers
  hist+coef-first              control, the other reorder
  hist+len+coef-first          control, coefficients first

All cells are value-identical re-spellings of the two constructors; no header,
no flag and no other function is touched.  Static anchor scoring only
(byteident verdicts); no fuzzing or mutation execution.

Modeled on playbook_small_patterns.py; the one deliberate deviation is that
the per-cell function-SIZE equality assert is relaxed to a function-SET
equality (the ctor pairs change size 112 -> 105 by design, and the size delta
is recorded per cell instead).
"""
import argparse
import hashlib
import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Keep the analysis script dis.py from shadowing stdlib exception reporting.
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT / 'tools']
import dis  # noqa: F401  (stdlib dis for exception reports)
sys.path[:] = _script_path
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b

REV = '438af9b7'
OUT_NAME = 'playbook-floatfir-ctor'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/dsp/FloatFIR.cpp', 'src/dsp/FloatIIR.cpp')

FF_HIST_OLD = '\thistory = (float *)sysdep_malloc(bufferLength * sizeof(float));\n\tif (history != 0)'
FF_HIST_NEW = ('\tfloat *hist = (float *)sysdep_malloc(bufferLength * sizeof(float));\n'
               '\thistory = hist;\n\tif (history != 0)')
FF_TOP_OLD = '\tunsigned int i;\n\n\ttaps'
FF_TOP_NEW = '\tunsigned int i;\n\tfloat *hist;\n\n\ttaps'
FF_HIST_TOP = ('\thist = (float *)sysdep_malloc(bufferLength * sizeof(float));\n'
               '\thistory = hist;\n\tif (history != 0)')
FF_ORDER_OLD = '\ttaps = nTaps & ~3u;\n\tcoefficients = coef;\n\tbufferLength = taps + blockSize;'
FF_ORDER_NEW = '\ttaps = nTaps & ~3u;\n\tbufferLength = taps + blockSize;\n\tcoefficients = coef;'
FF_COEF_OLD = '\tcoefficients = coef;\n\ttaps = nTaps & ~3u;\n\tbufferLength = taps + blockSize;'

II_HIST_OLD = '\tm_hist = (float *)sysdep_malloc(m_len * sizeof(float));'
II_HIST_NEW = ('\tfloat *hist = (float *)sysdep_malloc(m_len * sizeof(float));\n'
               '\tm_hist = hist;')
II_HIST_TOP = '\thist = (float *)sysdep_malloc(m_len * sizeof(float));\n\tm_hist = hist;'
II_TOP_OLD = 'FloatIIR::FloatIIR(unsigned ncoeff, float *coeff, unsigned blockSize)\n{\n'
II_TOP_NEW = 'FloatIIR::FloatIIR(unsigned ncoeff, float *coeff, unsigned blockSize)\n{\n\tfloat *hist;\n'
II_ORDER_OLD = '\tm_ncoeff = ncoeff & ~3u;\n\tm_coeff = coeff;\n\tm_len = m_ncoeff + blockSize;'
II_ORDER_NEW = '\tm_ncoeff = ncoeff & ~3u;\n\tm_len = m_ncoeff + blockSize;\n\tm_coeff = coeff;'
II_COEF_OLD = '\tm_coeff = coeff;\n\tm_ncoeff = ncoeff & ~3u;\n\tm_len = m_ncoeff + blockSize;'

CELLS = {
    'src/dsp/FloatFIR.cpp': [
        ('baseline', []),
        ('hist', [(FF_HIST_OLD, FF_HIST_NEW)]),
        ('len', [(FF_ORDER_OLD, FF_ORDER_NEW)]),
        ('hist+len', [(FF_HIST_OLD, FF_HIST_NEW), (FF_ORDER_OLD, FF_ORDER_NEW)]),
        ('hist+coef-first', [(FF_HIST_OLD, FF_HIST_NEW), (FF_ORDER_OLD, FF_COEF_OLD)]),
        ('hist-top', [(FF_HIST_OLD, FF_HIST_TOP), (FF_TOP_OLD, FF_TOP_NEW)]),
    ],
    'src/dsp/FloatIIR.cpp': [
        ('baseline', []),
        ('hist', [(II_HIST_OLD, II_HIST_NEW)]),
        ('len', [(II_ORDER_OLD, II_ORDER_NEW)]),
        ('hist+len', [(II_HIST_OLD, II_HIST_NEW), (II_ORDER_OLD, II_ORDER_NEW)]),
        ('hist+coef-first', [(II_HIST_OLD, II_HIST_NEW), (II_ORDER_OLD, II_COEF_OLD)]),
        ('hist-top', [(II_HIST_OLD, II_HIST_TOP), (II_TOP_OLD, II_TOP_NEW)]),
    ],
}


def variants(path, source):
    cells = {}
    for label, edits in CELLS[path]:
        text = source
        for old, new in edits:
            assert text.count(old) == 1, (path, label, old[:48])
            text = text.replace(old, new)
        cells[label] = text
    assert len(cells) == len(CELLS[path]) == len(set(cells.values()))
    return cells


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--domain', required=True)
    args = ap.parse_args()
    out = ROOT / 'build' / OUT_NAME
    out.mkdir(exist_ok=True)
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/' + f
             if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    hpaths = subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', REV, '--', 'include',
         'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(
        ['git', 'diff', '--name-only', REV, '--', 'include',
         'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip()
    local_roots = sorted({str(Path(p).parent) for p in SOURCE_PATHS})
    local_paths = subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', REV, '--'] + local_roots,
        cwd=ROOT, text=True).splitlines()
    local_headers = sorted({p for p in local_paths if p.endswith('.h')})
    if local_headers:
        assert not subprocess.check_output(
            ['git', 'diff', '--name-only', REV, '--'] + local_headers,
            cwd=ROOT, text=True).strip(), 'local header baseline drift'
    hpaths += local_headers
    headers = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in hpaths}
    result = {'revision': REV, 'domain': args.domain, 'config': config,
              'headers': headers, 'families': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    identity_shell = ('set -e; export PATH=' + shlex.quote(tc.GENTOO_COMPILER_PATH) +
                      ':$PATH; command -v g++; g++ --version; g++ -dumpmachine; '
                      'replay_as_path=$(g++ -print-prog-name=as); "$replay_as_path" --version')
    identity_command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', identity_shell]
    (out / 'cxx-identity-command.json').write_text(json.dumps(identity_command, indent=2) + '\n')
    with (out / 'cxx-identity.log').open('w') as log:
        subprocess.run(identity_command, stdout=log, stderr=subprocess.STDOUT, check=True)
    for path in SOURCE_PATHS:
        source = subprocess.check_output(['git', 'show', REV + ':' + path], cwd=ROOT, text=True)
        family = Path(path).stem
        fo = out / family
        fo.mkdir(exist_ok=True)
        retained = ROOT / 'build/tc_out' / (path.replace('/', '_') + '.o')
        saved = fo / 'retained.o'
        if not saved.exists():
            saved.write_bytes(retained.read_bytes())
        cells = variants(path, source)
        fr = {'source_path': path,
              'retained_hash': hashlib.sha256(saved.read_bytes()).hexdigest(),
              'cells': {}}
        result['families'][family] = fr
        for label, text in cells.items():
            cd = fo / label
            cd.mkdir(exist_ok=True)
            file = cd / Path(path).name
            file.write_text(text)
            for local in (ROOT / Path(path).parent).glob('*.h'):
                rel = str(local.relative_to(ROOT))
                contents = subprocess.check_output(['git', 'show', REV + ':' + rel], cwd=ROOT)
                assert hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() == headers[rel]
                (cd / local.name).write_bytes(contents)
            dst = '/work/' + family + '/' + label
            cell_flags = flags + list(DUMP_FLAGS)
            if file.suffix == '.cpp':
                cell_flags += shlex.split(next(x[6:] for x in config.splitlines()
                                               if x.startswith('cxx   ')))
            shell = tc.compile_shell(tc.GENTOO_COMPILER_PATH, cell_flags,
                                     dst + '/candidate.o', dst + '/' + file.name)
            assert shell.count('exec gcc -c ') == 1
            shell = shell.replace('exec gcc -c ', 'exec g++ -c ', 1)
            command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                      'cd ' + shlex.quote(dst) + ' && ' + shell]
            entry = {'source_hash': hashlib.sha256(text.encode()).hexdigest(),
                     'command': command}
            fr['cells'][label] = entry
            assert all(hashlib.sha256((ROOT / p).read_bytes()).hexdigest() == d
                       for p, d in headers.items())
            with (cd / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(command, stdout=log,
                                                       stderr=subprocess.STDOUT).returncode
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
            assert entry['compile_exit'] == 0, label
            obj = str(cd / 'candidate.o')
            entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals'] = {x.split()[-1]: x.split()[-2] for x in
                                subprocess.check_output(['nm', '-g', '--defined-only', obj],
                                                        text=True).splitlines()}
            entry['functions'] = sorted(b.sizes(obj))
            entry['verdicts'] = {n: b.verdict(*b.body(b.BLOB, n), *b.body(obj, n))
                                 for n in sorted(set(b.sizes(obj)) & set(b.sizes(b.BLOB)))}
            if label == 'baseline':
                assert Path(obj).read_bytes() == saved.read_bytes(), 'raw baseline mismatch'
                entry['baseline_reproduced'] = True
            else:
                base = fr['cells']['baseline']
                baseobj = str(fo / 'baseline/candidate.o')
                base_sizes = b.sizes(baseobj)
                assert set(entry['functions']) == set(base['functions']), \
                    'symbol set changed: ' + label
                entry['function_size_deltas'] = {n: b.sizes(obj)[n] - base_sizes[n]
                                                 for n in base_sizes
                                                 if b.sizes(obj)[n] != base_sizes[n]}
                entry['changed_bodies'] = [n for n in entry['functions']
                                           if b.body(obj, n) != b.body(baseobj, n)]
                entry['gains'] = [n for n, v in entry['verdicts'].items()
                                  if v[0] == 'EXACT' and base['verdicts'][n][0] != 'EXACT']
                entry['losses'] = [n for n, v in base['verdicts'].items()
                                   if v[0] == 'EXACT' and entry['verdicts'][n][0] != 'EXACT']
                for n in entry['changed_bodies']:
                    (cd / (n + '.dis')).write_text(
                        subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, n],
                                                text=True, stderr=subprocess.DEVNULL))
            exact = sum(v[0] == 'EXACT' for v in entry['verdicts'].values())
            print(family, label, 'exact', exact, '/', len(entry['verdicts']),
                  'gains', entry.get('gains', []), 'losses', entry.get('losses', []),
                  flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
