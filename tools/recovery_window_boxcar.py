#!/usr/bin/env python3
"""Recover the separately witnessed BOXCAR arm across all header consumers."""
import json
from collections import Counter
import re
import subprocess
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from elftools.elf.elffile import ELFFile

HEADER = 'dsplib/DspMath.h'
SYMBOL = '_Z12designWindowIfEv10WindowTypePT_j'
SOURCES = ('src/dsp/psd.cpp', 'src/pump/v90/Resampler.cpp',
           'src/pump/v90/V90Equalizer.cpp', 'src/pump/v90/V90Resampler.cpp',
           'src/pump/v90/V90SpectralVerifier.cpp')


def header(text):
    marker = '\tswitch (type) {\n\tcase WINDOW_HANNING:'
    assert text.count(marker) == 1
    text = text.replace(marker, '\tswitch (type) {\n\tcase WINDOW_BOXCAR:\n\t\tboxcar(w, n);\n\t\tbreak;\n\tcase WINDOW_HANNING:')
    old = ''' * The object reaches `boxcar` two ways -- case 0 tail-jumps to it and the
 * default CALLs it and returns -- which is a layout artefact of the compiler's
 * switch and not a behavioural difference.  One `default` covers both.'''
    new = ''' * The object has separate case 0 and default paths to `boxcar`: case 0
 * tail-jumps, while default calls and returns. Keep the explicit BOXCAR arm
 * even though both paths compute the same window.'''
    assert text.count(old) == 1
    return text.replace(old, new)


def consumers():
    """Quoted project includes, scoped to source/include and pinned by checks."""
    memo = {}
    def reaches(path, visiting):
        if path == d.ROOT / 'include' / HEADER:
            return True
        if path in memo:
            return memo[path]
        if path in visiting:
            return False
        hit = False
        for inc in re.findall(r'^\s*#\s*include\s*"([^"]+)"', path.read_text(), re.M):
            candidates = (path.parent / inc, d.ROOT / 'include' / inc)
            found = next((p.resolve() for p in candidates if p.is_file()), None)
            if found:
                hit |= reaches(found, visiting | {path})
        memo[path] = hit
        return hit
    found = sorted(str(p.relative_to(d.ROOT)) for p in (d.ROOT / 'src').rglob('*')
                   if p.suffix in ('.c', '.cpp') and reaches(p, set()))
    assert found == sorted(SOURCES), found
    return found


def overlays(path, label):
    original = subprocess.check_output(['git', 'show', d.REV + ':include/' + HEADER], cwd=d.ROOT, text=True)
    return {} if label == 'baseline' else {HEADER: header(original)}


def audit():
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    report = {'header_consumers': consumers(), 'cells': {}}
    for family, data in results['families'].items():
        a, b = (root / family / label / 'candidate.o' for label in ('baseline', 'boxcar-arm'))
        old, new = Object(a).canonical(), Object(b).canonical()
        code = lambda n: n == '.text' or n.startswith('.gnu.linkonce.t.')
        assert set(old[0]) == set(new[0]) and old[2] == new[2]
        assert {n:v for n,v in old[1].items() if not code(n)} == {
            n:v for n,v in new[1].items() if not code(n)}
        assert {n:v for n,v in allocated(a).items() if not code(n)} == {
            n:v for n,v in allocated(b).items() if not code(n)}
        changed = [n for n in old[0] if old[0][n] != new[0][n]]
        row = data['cells']['boxcar-arm']
        emitting = family in ('psd', 'Resampler')
        assert changed == ([SYMBOL] if emitting else [])
        assert row['changed_bodies'] == changed
        assert row['gains'] == ([SYMBOL] if emitting else []) and row['losses'] == []
        if emitting:
            assert data['cells']['baseline']['verdicts'][SYMBOL] == ['SIZE', 17]
            assert row['verdicts'][SYMBOL] == ['EXACT', 0]
            assert d.b.sizes(str(b))[SYMBOL] == 116
            assert new[3][SYMBOL] - old[3][SYMBOL] == Counter({(2, ('_Z6boxcarIfEvPT_j', -4)): 1})
            assert not old[3][SYMBOL] - new[3][SYMBOL]
        else:
            assert a.read_bytes() == b.read_bytes()
        for owner in old[3]:
            if owner != SYMBOL:
                assert old[3][owner] == new[3][owner], (family, owner)
        report['cells'][family] = {'bodies': len(old[0]), 'changed': changed,
            'exact_gains': row['gains'], 'exact_losses': row['losses'],
            'verdict': row['verdicts'].get(SYMBOL),
            'changed_relocation_consumers': [n for n in old[3] if old[3][n] != new[3][n]],
            'metadata': metadata(a, b),
            'object_sha256': row['object_hash']}
    report['body_copies'] = sum(c['bodies'] for c in report['cells'].values())
    assert report['body_copies'] == 103
    report['changed_body_copies'] = 2
    report['unchanged_body_copies'] = 101
    report['unique_exact_gains'] = [SYMBOL]
    report['unique_exact_losses'] = []
    report['gained_original_function_bytes'] = 116
    (root / 'window-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


def metadata(a, b):
    def read(path):
        with path.open('rb') as stream:
            elf = ELFFile(stream)
            sections = {s.name: {k:s[k] for k in ('sh_type', 'sh_flags', 'sh_addr',
                'sh_offset', 'sh_size', 'sh_link', 'sh_info', 'sh_addralign', 'sh_entsize')}
                for s in elf.iter_sections()}
            symbols = [(s.name, s['st_value'], s['st_size'], s['st_shndx'],
                        s['st_info']['bind'], s['st_info']['type'], s['st_other']['visibility'])
                       for s in elf.get_section_by_name('.symtab').iter_symbols()]
        return sections, symbols
    old, new = read(a), read(b)
    assert old[0].keys() == new[0].keys() and len(old[1]) == len(new[1])
    symbols = [{'name': x[0], 'before': x[1:], 'after': y[1:]}
               for x,y in zip(old[1], new[1]) if x != y]
    assert all(s['name'] == SYMBOL and s['before'][1] == 99 and s['after'][1] == 116 for s in symbols)
    assert all(s['before'][:1] + s['before'][2:] == s['after'][:1] + s['after'][2:] for s in symbols)
    sections = {n: {k:[old[0][n][k],new[0][n][k]] for k in old[0][n]
                    if old[0][n][k] != new[0][n][k]}
                for n in old[0] if old[0][n] != new[0][n]}
    assert all(set(v) <= {'sh_size', 'sh_offset'} for v in sections.values())
    for n,row in sections.items():
        if 'sh_size' in row:
            assert n in ('.gnu.linkonce.t.' + SYMBOL, '.rel.gnu.linkonce.t.' + SYMBOL)
            assert row['sh_size'][1] - row['sh_size'][0] == (8 if n.startswith('.rel.') else 17)
    return {'symbol_records': len(old[1]), 'section_records': len(old[0]),
            'changed_symbols': symbols, 'changed_sections': sections}


def production_control():
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    for family, data in results['families'].items():
        selected = root / family / 'boxcar-arm'
        out = root / family / 'production-control'
        out.mkdir(exist_ok=True)
        source = Path(data['source_path']).name
        (out / source).write_bytes((d.ROOT / data['source_path']).read_bytes())
        target = out / 'include' / HEADER
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes((d.ROOT / 'include' / HEADER).read_bytes())
        command = [s.replace('/' + family + '/boxcar-arm', '/' + family + '/production-control')
                   for s in data['cells']['boxcar-arm']['command']]
        with (out / 'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        assert (out / 'candidate.o').read_bytes() == (selected / 'candidate.o').read_bytes()
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    print('production control: 5/5 complete objects raw-identical')


if __name__ == '__main__':
    d.REV, d.OUT_NAME = '6fe0b1e42', 'window-boxcar'
    d.SOURCE_PATHS, d.DUMP_FLAGS = SOURCES, ('-v', '-save-temps', '-da')
    d.variants = lambda path, text: {'baseline': text, 'boxcar-arm': text}
    d.HEADER_OVERLAYS = overlays
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--production-control']:
        production_control()
    else:
        consumers()
        if '--historical-headers' not in sys.argv:
            sys.argv.append('--historical-headers')
        d.main()
        audit()
