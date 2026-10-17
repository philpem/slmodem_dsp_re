#!/usr/bin/env python3
"""Replay the six distinct silence guard controls on complete period TUs.

Reference +0x34 compares the low word with3; +0x3e tests the widened value
against zero. Cross capture/comparison width with combined versus separate
return boundaries. Two four-cell experiments originally shared two controls.
Supply --baseline-dir for the archived5bfa2f09c period object/configuration.
"""
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_fax_debug_audit import Object
from recovery_generator_separation import named_data


def variants(path, source):
    start, end, fn = d.function(source, 'silence_progress')
    old = '\tif (lvl > 3 || lvl == 0)\n\t\treturn;'
    assert fn.count(old) == fn.count('unsigned short lvl;') == 1
    wide = fn.replace('unsigned short lvl;', 'unsigned int lvl;')
    forms = {
        'baseline': fn,
        'wide': wide,
        'first-narrow': wide.replace('if (lvl > 3 || lvl == 0)',
                                    'if ((unsigned short)lvl > 3 || lvl == 0)'),
        'both-narrow': wide.replace('if (lvl > 3 || lvl == 0)',
                                   'if ((unsigned short)lvl > 3 || (unsigned short)lvl == 0)'),
        'mixed-split': wide.replace(old, '\tif ((unsigned short)lvl > 3)\n\t\treturn;\n\tif (lvl == 0)\n\t\treturn;'),
        'short-split': fn.replace(old, '\tif (lvl > 3)\n\t\treturn;\n\tif (lvl == 0)\n\t\treturn;')}
    assert len(forms) == len(set(forms.values())) == 6
    return {label: source[:start]+body+source[end:] for label, body in forms.items()}


def audit():
    root = d.ROOT/'build/silence-guards-293/silence'
    before = root/'baseline/candidate.o'
    baseline = Object(before).canonical()
    reports = {}
    for folder in sorted(root.iterdir()):
        candidate = folder/'candidate.o'
        if not candidate.exists():
            continue
        current = Object(candidate).canonical()
        assert baseline[0].keys() == current[0].keys()
        changed = [name for name in baseline[0] if baseline[0][name] != current[0][name]]
        assert set(changed) <= {'silence_progress'}
        assert baseline[1:] == current[1:]
        assert named_data(before) == named_data(candidate)
        reports[folder.name] = {'bodies': len(current[0]), 'changed': changed,
            'verdict': d.b.verdict(*d.b.body(d.b.BLOB, 'silence_progress'),
                                    *d.b.body(str(candidate), 'silence_progress'))}
    assert len(reports) == 6 and all(r['bodies'] == 5 for r in reports.values())
    assert reports['short-split']['verdict'] == reports['mixed-split']['verdict'] == ('EXACT', 0)
    assert (root/'short-split/candidate.o').read_bytes() == (root/'mixed-split/candidate.o').read_bytes()
    # A changed writable threshold must invalidate the noncode audit.
    negative = root/'data-negative.o'
    raw = bytearray(before.read_bytes())
    with before.open('rb') as stream:
        table = ELFFile(stream).get_section_by_name('.data')
        assert table['sh_size'] == 16
        raw[table['sh_offset']+4] ^= 1
    negative.write_bytes(raw)
    assert Object(negative).canonical()[1:] != baseline[1:]
    negative.unlink()
    (root.parent/'audit.json').write_text(json.dumps(reports, indent=2)+'\n')
    print('6/6 controls;30 body verdicts;4 unchanged bystanders per cell;data/bindings/consumers equal')
    print('2 exact source controls raw-identical;1/1 corrupt-data negative detected')


if __name__ == '__main__':
    import sys
    d.REV = '5bfa2f09c'
    d.SOURCE_PATHS = ('src/service/silence.c',)
    d.OUT_NAME = 'silence-guards-293'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-tree-original')
    d.variants = variants
    if sys.argv[1:] == ['--audit']:
        audit()
    else:
        d.main()
