#!/usr/bin/env python3
"""Eight-cell full-TU control of witnessed V92 encoder case ownership.

This is the historical source/profile cross. The no-auto-inline profile stays
diagnostic; current case-boundary adoption is audited separately by
recovery_v92_boundary_adoption.py, without reopening this spelling domain.
Use an archived canonical Gentoo object tree with its .build-config:

  mkdir -p build/conv-owner-next293
  python3 tools/recovery_v92_case_owners.py --domain case-owned-tables \\
      --baseline-dir build/archived-conv-baseline
  python3 tools/recovery_v92_case_owners.py --audit

The source revision is frozen below; current headers must match it. The driver
records complete compiler commands, identities, hashes and all eight bodies.
"""
import playbook_small_patterns as d
import json
import sys
import hashlib
import subprocess
from collections import Counter

d.REV = 'c118236196b1e6a6bd4f7aeafd6458cea1ad544c'
d.OUT_NAME = 'conv-owner-next293/reproduction'
d.SOURCE_PATHS = ('src/pump/v90/V92ConvEncoder.cpp',)
d.DUMP_FLAGS = ('-da', '-v', '-save-temps', '-fdump-tree-original',
                '-fdump-tree-inlined', '-fdump-tree-optimized')


def owned(source, direct):
    start, end, old = d.function(source, 'V92ConvolutionEncoder::process')
    text = old.replace('\tint ns;\n\tint i;', '\tint ns;\n\tint result;\n\tint i;')
    for expression in ('r % 4', '(r & 3) | ((r & 8) >> 1)', 'r % 16'):
        before = '\t\ti = ' + expression + ';\n\t\tbreak;'
        after = ('\t\ti = ' + expression + ';\n'
                 '\t\tns = nextState[(state << 4) + i];\n'
                 '\t\tstate = ns;\n')
        after += ('\t\treturn output[(ns << 4) + i];' if direct else
                  '\t\tresult = output[(ns << 4) + i];\n\t\tbreak;')
        assert text.count(before) == 1
        text = text.replace(before, after)
    before = ('\tns = nextState[(state << 4) + i];\n'
              '\tstate = ns;\n\treturn output[(ns << 4) + i];')
    assert text.count(before) == 1
    text = text.replace(before, '\tstate = ns;\n\treturn result;')
    return source[:start] + text + source[end:]


def variants(path, source):
    default_only = owned(source, False)
    before = '\t\tbreak;\n\t}\n\n\tstate = ns;\n\treturn result;'
    after = '\t\tbreak;\n\tdefault:\n\t\tstate = ns;\n\t\tbreak;\n\t}\n\n\treturn result;'
    assert default_only.count(before) == 1
    default_only = default_only.replace(before, after)
    return {'baseline': source, 'baseline-no-auto-inline': source,
            **{label + suffix: owned(source, direct)
               for label, direct in (('case-result-common-tail', False),
                                     ('case-direct-return', True))
               for suffix in ('', '-no-auto-inline')},
            'default-only-state': default_only,
            'default-only-state-no-auto-inline': default_only}


d.variants = variants
d.CELL_FLAGS = lambda path, label: ('-fno-inline-functions',) if label.endswith('-no-auto-inline') else ()


def audit():
    from gcc3_value_carriers_audit import inspect
    from elftools.elf.elffile import ELFFile
    root = d.ROOT / 'build' / d.OUT_NAME
    results = json.loads((root / 'results.json').read_text())
    family = results['families']['V92ConvEncoder']
    cells = family['cells']
    folder = root / 'V92ConvEncoder'
    baseline = inspect(folder / 'baseline/candidate.o')
    target = '_ZN21V92ConvolutionEncoder7processEPi'
    expected = {'baseline': ['SIZE', 263],
                'baseline-no-auto-inline': ['SIZE', 25],
                'case-result-common-tail': ['SIZE', 218],
                'case-result-common-tail-no-auto-inline': ['EXACT', 0],
                'case-direct-return': ['SIZE', 193],
                'case-direct-return-no-auto-inline': ['SIZE', 68],
                'default-only-state': ['SIZE', 202],
                'default-only-state-no-auto-inline': ['SIZE', 68]}
    assert set(cells) == set(expected)
    assert cells['baseline']['baseline_reproduced']
    assert results['revision'] == d.REV
    assert 'DSPLIB_REPRODUCE_BUGS' in results['config']
    rows = []
    for label, verdict in expected.items():
        cell = cells[label]
        obj = inspect(folder / label / 'candidate.o')
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert obj[key] == baseline[key], (label, key)
        assert len(cell['verdicts']) == len(cell['functions']) == 8
        assert cell['verdicts'][target] == verdict
        assert cell.get('changed_bodies', []) == ([] if label == 'baseline' else [target])
        assert cell.get('losses', []) == []
        assert cell.get('gains', []) == ([target] if verdict[0] == 'EXACT' else [])
        relocs = Counter(d.b.body(str(folder / label / 'candidate.o'), target)[1].values())
        prefix = '_ZN21V92ConvolutionEncoder'
        expected_calls = Counter({('R_386_PC32', ('symbol', prefix + '10inverseMapEPi', 4294967292)): 1})
        expected_inline = Counter({('R_386_32', ('symbol', prefix + '16subsetLabelTableE', 0)): 2,
                                   ('R_386_32', ('symbol', prefix + '14cosetMapping4DE', 0)): 1})
        assert relocs == (expected_calls if label.endswith('-no-auto-inline') else expected_inline), label
        rows.append(dict(cell=label, process_bytes=obj['text_positions'][target][1],
                         verdict=verdict, exact=sum(v[0] == 'EXACT' for v in cell['verdicts'].values())))
    # Fire the noncode comparison on a known byte change before trusting its
    # unchanged-data result. This object is inspected only, never executed.
    original = folder / 'baseline/candidate.o'
    changed = bytearray(original.read_bytes())
    with original.open('rb') as stream:
        data_offset = ELFFile(stream).get_section_by_name('.data')['sh_offset']
    changed[data_offset] ^= 1
    negative = root / 'changed-data-negative.o'
    negative.write_bytes(changed)
    assert inspect(negative)['allocated'] != baseline['allocated']
    (root / 'audit.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('V92 case ownership: 8/8 cells, 64 body verdicts; unchanged data, bindings and noncode relocations; only process changes; call/inline relocation sets verified; no exact losses; changed-data negative control fired')


def comment_control():
    """Require the corrected source commentary to leave the full object raw-identical."""
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    cell = root / 'comment-control'
    cell.mkdir(exist_ok=True)
    source = d.ROOT / d.SOURCE_PATHS[0]
    (cell / source.name).write_bytes(source.read_bytes())
    command = result['families']['V92ConvEncoder']['cells']['baseline']['command']
    command = [x.replace('/V92ConvEncoder/baseline', '/comment-control') for x in command]
    command[-1] = command[-1].replace('cd /work/comment-control && ', 'cd /work/comment-control || exit 1; ')
    (cell / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (cell / 'compile.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    actual = (cell / 'candidate.o').read_bytes()
    assert actual == (root / 'V92ConvEncoder/baseline/candidate.o').read_bytes()
    print('Comment-only correction: raw full Gentoo object identical; SHA256', hashlib.sha256(actual).hexdigest())


if __name__ == '__main__':
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--comment-control']:
        comment_control()
    else:
        d.main()
