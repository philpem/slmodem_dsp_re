#!/usr/bin/env python3
"""Replay the witnessed Resampler residue loop and phase-loop ownership cross."""
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path, source):
    residue = '\t\twhile (n_-- != 0)\t\t\t\t\t\\\n\t\t\ta_ += *h_++ * *c_++;\t\t\t\t\\\n'
    zero = '\t\twhile (n_ != 0) {\t\t\t\t\t\\\n\t\t\ta_ += *h_++ * *c_++;\t\t\t\t\\\n\t\t\tn_--;\t\t\t\t\t\t\\\n\t\t}\t\t\t\t\t\t\t\\\n'
    phase = '\t\twhile (phase >= phases) {\n\t\t\tphase -= phases;\n\t\t\tinputCredit++;\n\t\t}'
    guarded = '\t\tif (phase >= phases) {\n\t\t\tdo {\n\t\t\t\tphase -= phases;\n\t\t\t\tinputCredit++;\n\t\t\t} while (phase >= phases);\n\t\t}'
    assert source.count(residue) == source.count(phase) == 1
    cells = {'baseline': source}
    for countdown, guard in itertools.product((False, True), repeat=2):
        if not (countdown or guard):
            continue
        text = source.replace(residue, zero) if countdown else source
        if guard:
            text = text.replace(phase, guarded)
        label = '-'.join(n for n, selected in (('zero-countdown', countdown), ('guarded-phase', guard)) if selected)
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


def audit():
    # Reuse the complete-object audit definitions without running their old
    # batch entry points. These are the same helpers used by the batch20 audits.
    from elftools.elf.elffile import ELFFile
    from elftools.elf.relocation import RelocationSection
    scope = dict(ELFFile=ELFFile, RelocationSection=RelocationSection,
                 b=driver.b)
    text = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
    exec(text[text.index('def inspect'):text.index('reports=')], scope)
    text = (driver.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
    exec(text[text.index('def nontext'):text.index('\nreports=')], scope)
    root = driver.ROOT/'build/resampler14'
    family = root/'Resampler'
    cells = json.loads((root/'results.json').read_text())['families']['Resampler']['cells']
    assert len(cells) == 4
    baseline = family/'baseline/candidate.o'
    metadata = scope['inspect'](baseline)
    nontext = scope['nontext'](baseline)
    target = '_ZN9Resampler8resampleEPKfjPfRj'
    header = ';; Function void Resampler::resample(const float*, unsigned int, float*, unsigned int&)\n'
    reports = {}
    for label, cell in cells.items():
        obj = family/label/'candidate.o'
        assert scope['inspect'](obj) == metadata, (label, 'metadata/data')
        assert scope['nontext'](obj) == nontext, (label, 'nontext relocations/constants')
        assert not cell.get('gains') and not cell.get('losses')
        assert set(cell.get('changed_bodies', [])) <= {target}
        assert len(cell['functions']) == 25
        counts = {}
        for stage in ('27.flow2', '28.peephole2', '29.ce3', '30.rnreg',
                      '31.bbro', '33.sched2', '34.stack', '35.mach'):
            text = (family/label/('Resampler.cpp.'+stage)).read_text()
            assert text.count(header) == 1
            body = text.split(header)[1].split('\n;; Function ')[0]
            counts[stage] = body.count('(compare:CCFP ')
        assert all(counts[s] == 3 for s in counts if s not in ('34.stack', '35.mach'))
        expected = 3 if label == 'baseline' else 4
        assert counts['34.stack'] == counts['35.mach'] == expected
        asm = subprocess.check_output([sys.executable, str(driver.ROOT/'tools/dis.py'), str(obj), target, '--plain'], text=True, stderr=subprocess.DEVNULL)
        assert len(re.findall(r'\bfcom\s', asm)) == expected
        (family/label/'resample.dis').write_text(asm)
        reports[label] = dict(functions=25, unchanged_bystanders=24,
                             exact_names=16, target_size=driver.b.sizes(str(obj))[target],
                             target_verdict=cell['verdicts'][target],
                             compare_counts=counts, final_fcoms=expected)
    result = dict(valid_cells=4, function_verdicts=100, bystander_verdicts=96,
                  exact_gains=[], exact_losses=[], reports=reports,
                  original_target_size=driver.b.sizes(driver.b.BLOB)[target],
                  conclusion='Extra FCOM is cloned during x87 stack conversion; no narrowing deficit or source predicate deficit demonstrated.')
    (root/'complete-tu-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('4/4 complete-TU audits; 100 function verdicts, 96 unchanged bystanders; data, bindings and positioned relocations equal; 0 exact gains/losses')
    print('24/24 pre-stack counts are 3; stack conversion changes 3/4 cells to 4 comparisons; 4/4 assembly counts agree')


if __name__ == '__main__':
    if sys.argv[1:] == ['--audit']:
        audit()
        sys.exit(0)
    driver.REV = '239b234c0'
    driver.OUT_NAME = 'resampler14'
    driver.SOURCE_PATHS = ('src/pump/v90/Resampler.cpp',)
    driver.DUMP_FLAGS = ('-da',)
    driver.variants = variants
    driver.main()
