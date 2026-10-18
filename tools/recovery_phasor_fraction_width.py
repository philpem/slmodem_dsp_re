#!/usr/bin/env python3
"""Replay the bounded phasor fraction-width cross on the Gentoo compiler."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
# Avoid tools/dis.py shadowing the standard library during ELF imports.
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT / 'tools']
import dis
sys.path[:] = _script_path
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b
from elftools.elf.elffile import ELFFile

REVISION = 'a0c079f61'
SOURCE = 'src/dsp/fpm_phasor.c'
OBJECT = 'src_dsp_fpm_phasor.c.o'


def metadata(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        sections = {s.name: [s['sh_type'], s['sh_flags'], s['sh_size'],
                            hashlib.sha256(s.data()).hexdigest()]
                    for s in elf.iter_sections()
                    if s.name not in ('.text', '.rel.text', '.symtab',
                                      '.shstrtab', '.strtab')}
        exports = sorted([s.name, s['st_info']['bind'], s['st_info']['type'],
                          s['st_other']['visibility']]
                         for s in elf.get_section_by_name('.symtab').iter_symbols()
                         if s['st_info']['bind'] != 'STB_LOCAL')
    return {'nontext_sections': sections, 'exports': exports}


def first_mask_dump(directory):
    for path in sorted(directory.glob('fpm_phasor.c.*')):
        text = path.read_text()
        match = re.search(r'^;; Function FPM_phasor_demod\b', text, re.M)
        if match and re.search(r'const_int 31\b', text[match.start():]):
            return path.name
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, required=True,
                        help='frozen baseline objects and .build-config')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    config = (args.objects / '.build-config').read_text()
    lines = config.splitlines()
    image = next(line[6:] for line in lines if line.startswith('image '))
    flags = shlex.split(next(line[6:] for line in lines if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else
             '/src/tools/toolchain/period_compat.h'
             if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    flags += ['-da']
    source = subprocess.check_output(['git', 'show', REVISION + ':' + SOURCE],
                                     cwd=ROOT, text=True)
    cells = [('baseline', False, False), ('frac-short', False, True),
             ('phase-ushort', True, False), ('both', True, True)]
    generated = {}
    for name, phase, frac in cells:
        text = source
        if phase:
            text = text.replace('phasor_split(int phase,',
                                'phasor_split(unsigned short phase,', 1)
            text = text.replace('int idx = phase >> 5;',
                                'int idx = (short)phase >> 5;', 1)
        if frac:
            text = text.replace('int frac = phase - (idx << 5);',
                                'short frac = (short)(phase - (idx << 5));', 1)
        generated[name] = text
    generated['hi-split'] = source.replace('phasor_split(int phase,',
                                           'phasor_split(short phase,', 1).replace(
        'int idx = phase >> 5;', 'short idx = phase >> 5;', 1).replace(
        'int frac = phase - (idx << 5);',
        'short frac = (short)(phase - (idx << 5));', 1)
    assert len(set(generated.values())) == 5, 'generator did not change all five cells'
    report = {'source_revision': REVISION, 'config': config, 'cells': {},
              'headers': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                          for name in ['include/dsplib/fpm_phasor.h',
                                       'tools/toolchain/period_compat.h']},
              'limits': 'Four crossed cells and one HI-mode mechanism control; '
                        'phase-ushort is diagnostic, not a behavioral candidate. '
                        'No differential tests are run by this replay.'}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    baseline = out / 'baseline/fpm_phasor.o'
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(exist_ok=True)
        (directory / 'fpm_phasor.c').write_text(text)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                tc.GENTOO_COMPILER_PATH, flags, target + '/fpm_phasor.o',
                target + '/fpm_phasor.c')]
        with (directory / 'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        obj = directory / 'fpm_phasor.o'
        if name == 'baseline':
            assert obj.read_bytes() == (args.objects / OBJECT).read_bytes(), \
                'unchanged baseline does not reproduce frozen object'
        names = sorted(b.sizes(str(baseline)))
        assert sorted(b.sizes(str(obj))) == names and len(names) == 3
        assert metadata(obj) == metadata(baseline), 'nontext or exported binding drift'
        report['cells'][name] = {
            'command': command, 'source_sha256': hashlib.sha256(text.encode()).hexdigest(),
            'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
            'shared_symbols': len(names), 'sizes': b.sizes(str(obj)),
            'verdicts': {n: b.verdict(*b.body(b.BLOB, n), *b.body(str(obj), n))
                         for n in names},
            'changed_bodies': [n for n in names
                               if b.body(str(obj), n) != b.body(str(baseline), n)],
            'first_fraction_mask_dump': first_mask_dump(directory),
            'nontext_and_exports_identical': True}
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(name, '3/3 symbols:', report['cells'][name]['verdicts'], flush=True)


if __name__ == '__main__':
    main()
