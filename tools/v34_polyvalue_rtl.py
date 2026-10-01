#!/usr/bin/env python3
"""Bounded full-TU/standalone polyValue compiler investigation; no harnesses."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b

FORMS = {
    'retained': '-21 * (int)k * k + 837 * k - 354',
    'square': '-21 * ((int)k * k) + 837 * k - 354',
    'factored': '(int)k * (837 - 21 * (int)k) - 354',
}
DOMAIN = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5928763753'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config', type=Path, required=True,
                    help='retained period .build-config')
    ap.add_argument('--blob', type=Path, required=True)
    args = ap.parse_args()
    config = args.config.read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines()
                            if x.startswith('flags ')))
    # GCC 3.4 dumps to cwd, independent of the output object's path.
    # Absolute include paths let every compile run inside its own cell.
    flags = ['-I/src/include' if x == '-Iinclude' else
             '/src/' + x if x == 'tools/toolchain/period_compat.h' else x
             for x in flags]
    work = ROOT / 'build/v34-polyvalue-rtl'
    work.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/pump/v34/V34hshak.c').read_text()
    old = '\treturn (short)(' + FORMS['retained'] + ');'
    assert source.count(old) == 1
    result = {'domain': DOMAIN, 'config': config,
              'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                                  cwd=ROOT, text=True).strip(),
              'source_sha256': digest(ROOT / 'src/pump/v34/V34hshak.c'),
              'header_sha256': {str(p.relative_to(ROOT)): digest(p)
                               for p in sorted((ROOT / 'include').rglob('*.h'))},
              'cells': {}}
    manifest = work / 'results.json'
    def save():
        manifest.write_text(json.dumps(result, indent=2) + '\n')
    # Run with stdout redirected to an artifact log to retain tool identity.
    # Shared helper executes the assembler chosen by GCC.
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    cells = [(context, form, rename)
             for context in ('full', 'toy')
             for form in FORMS for rename in ('on', 'off')]
    cells += [('repeat', 'retained', 'on')]
    for context, form, rename in cells:
        name = '-'.join((context, form, rename))
        cell = work / name
        cell.mkdir(exist_ok=True)
        if context == 'toy':
            text = 'int polyValue(short k)\n{\n\treturn (short)(' + FORMS[form] + ');\n}\n'
        else:
            text = source.replace(old, '\treturn (short)(' + FORMS[form] + ');')
        src = cell / 'V34hshak.c'
        src.write_text(text)
        extra = [] if rename == 'on' else ['-fno-rename-registers']
        container = '/work/' + name
        command = tc.docker_prefix(image, ROOT, work, True) + [
            '/bin/sh', '-c', 'cd ' + shlex.quote(container) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH,
                flags + extra + ['-da'], container + '/V34hshak.o',
                container + '/V34hshak.c')]
        entry = {'command': command, 'source_sha256': digest(src)}
        result['cells'][name] = entry
        save()
        with (cell / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(command, stdout=log,
                stderr=subprocess.STDOUT).returncode
        save()
        if entry['compile_exit']:
            raise SystemExit('invalid compile: ' + name)
        obj = cell / 'V34hshak.o'
        entry['object_sha256'] = digest(obj)
        raw, rel = b.body(str(obj), 'polyValue')
        reference, reloc = b.body(str(args.blob), 'polyValue')
        entry['polyValue'] = {'bytes': len(raw), 'verdict': b.verdict(reference, reloc, raw, rel)}
        entry['rtl_files'] = [p.name for p in sorted(cell.glob('V34hshak.c.*'))]
        assert entry['rtl_files'], 'invalid run: missing RTL dumps'
        dis = subprocess.check_output(['objdump', '-dr', '--disassemble=polyValue',
                                       str(obj)], text=True)
        (cell / 'polyValue.dis').write_text(dis)
        entry['globals'] = subprocess.check_output(['nm', '-g', '--defined-only',
            str(obj)], text=True)
        entry['global_bindings'] = {line.split()[-1]: line.split()[-2]
                                   for line in entry['globals'].splitlines()}
        print(name, entry['polyValue'], 'RTL files', len(entry['rtl_files']), flush=True)
        save()
    first = work / 'full-retained-on/V34hshak.o'
    repeat = work / 'repeat-retained-on/V34hshak.o'
    assert first.read_bytes() == repeat.read_bytes(), 'baseline repeat drift'
    result['baseline_repeat_byte_identical'] = True
    for context in ('full', 'toy'):
        original = result['cells'][context + '-retained-on']['global_bindings']
        assert all(entry['global_bindings'] == original
                   for name, entry in result['cells'].items()
                   if name.startswith(context + '-')), 'global surface changed'
    for form in FORMS:
        for rename in ('on', 'off'):
            x, xr = b.body(str(work / f'full-{form}-{rename}/V34hshak.o'), 'polyValue')
            y, yr = b.body(str(work / f'toy-{form}-{rename}/V34hshak.o'), 'polyValue')
            result['cells'][f'toy-{form}-{rename}']['full_tu_verdict'] = b.verdict(x, xr, y, yr)
        x, xr = b.body(str(work / f'full-{form}-on/V34hshak.o'), 'polyValue')
        y, yr = b.body(str(work / f'full-{form}-off/V34hshak.o'), 'polyValue')
        result['cells'][f'full-{form}-off']['rename_effect'] = b.verdict(x, xr, y, yr)
    # Score every shared body; a target gain alone cannot hide TU collateral.
    for name, entry in result['cells'].items():
        if not name.startswith('full-'):
            continue
        obj = work / name / 'V34hshak.o'
        shared = sorted(set(b.sizes(str(obj))) & set(b.sizes(str(args.blob))))
        entry['shared_verdicts'] = {}
        for symbol in shared:
            x, xr = b.body(str(args.blob), symbol)
            y, yr = b.body(str(obj), symbol)
            entry['shared_verdicts'][symbol] = b.verdict(x, xr, y, yr)
    for name in result['cells']:
        cell = work / name
        focused = cell / 'focused-rtl'
        focused.mkdir(exist_ok=True)
        for dump in cell.glob('V34hshak.c.*'):
            text = dump.read_text(errors='replace')
            for symbol in ('polyValue', 'dftRetrainDetInit', 'detectRetrainReq'):
                match = re.search(r'^;; Function ' + symbol +
                                  r'(?:\s|\().*?(?=^;; Function |\Z)',
                                  text, re.M | re.S)
                if match:
                    (focused / (symbol + '.' + dump.name.split('.c.', 1)[1])).write_text(match.group())
    save()
    print('12 cells + 1 repeated baseline completed; baseline repeat byte-identical')


if __name__ == '__main__':
    main()
