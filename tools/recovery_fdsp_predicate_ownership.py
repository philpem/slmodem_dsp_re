#!/usr/bin/env python3
"""Replay witnessed FDSP predicate/count ownership on complete period TUs.

The earlier batch50 controls declined these boundaries for lack of an exact
function. This domain asks the narrower question: which original operations
can be recovered without changing other bodies or inventing a runtime defect?
"""
import json
import subprocess
from pathlib import Path
import playbook_small_patterns as driver
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data


def variants(path, source):
    start, end, original = driver.function(source, 'EchoCanceler')
    result = {'baseline': source}
    fn = original.replace('int quiet = 0;', 'unsigned int quiet = 0;')
    assert fn != original
    fn = fn.replace('float x, e, peak;', 'float x, e, peak;\n\t\tint low;')
    old = 'if (fabs(x) < peak)\n\t\t\tquiet++;'
    assert fn.count(old) == 1
    fn = fn.replace(old, 'low = fabs(x) < peak;\n\t\tquiet += low != 0;')
    assert fn.count('mu != 0.0f && fabs(x) < peak') == 1
    fn = fn.replace('mu != 0.0f && fabs(x) < peak', 'mu != 0.0f && low')
    result['captured-quiet'] = source[:start] + fn + source[end:]
    old = '\t\tpeak = FDSP_FABS(hist[pos]);'
    assert fn.count(old) == 1
    fn = fn.replace(old, '\t\t{\n\t\tconst float *h = hist + pos;\n'
                        '\t\tpeak = FDSP_FABS(h[0]);')
    fn = fn.replace('FDSP_FABS(hist[pos + k])', 'FDSP_FABS(h[k])')
    fn = fn.replace('\n\t\tpeak = peak * 0.5f;', '\n\t\t}\n\t\tpeak = peak * 0.5f;')
    result['captured-quiet-history-owner'] = source[:start] + fn + source[end:]
    assert len(set(result.values())) == 3
    # Freeze the accepted first-stage source; later nested-guard recovery has
    # its own replay and must not silently change this historical control.
    result['working-source'] = subprocess.check_output(
        ['git', 'show', '5d720ce7f:'+path], cwd=driver.ROOT, text=True)
    return result


if __name__ == '__main__':
    driver.REV = '239b234c0'
    driver.OUT_NAME = 'recovery-fdsp-predicate-ownership'
    driver.SOURCE_PATHS = ('src/service/Fdspkrnl.c',)
    driver.DUMP_FLAGS = ('-da',)
    driver.variants = variants
    driver.main()
    root = driver.ROOT/'build'/driver.OUT_NAME/'Fdspkrnl'
    baseline = root/'baseline/candidate.o'
    original = Object(baseline).canonical()
    data = {k: v for k, v in allocated(baseline).items() if k != '.text'}
    report = {}
    for label in ('captured-quiet', 'captured-quiet-history-owner', 'working-source'):
        candidate = root/label/'candidate.o'
        current = Object(candidate).canonical()
        changed = [n for n in original[0] if original[0][n] != current[0][n]]
        assert changed == ['EchoCanceler'], changed
        assert original[1:] == current[1:], 'data, binding or resolved consumer drift'
        assert named_data(baseline) == named_data(candidate)
        assert data == {k: v for k, v in allocated(candidate).items() if k != '.text'}
        report[label] = {'emitted_functions': len(original[0]), 'changed': changed,
                         'unchanged_bystanders': len(original[0])-1,
                         'allocated_nontext_sections': len(data),
                         'named_data_records': len(named_data(candidate)),
                         'data_bindings_consumers_equal': True}
    assert (root/'working-source/candidate.o').read_bytes() == (root/'captured-quiet-history-owner/candidate.o').read_bytes()
    (root.parent/'complete-tu-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('full-TU audit: 3 candidates, 18 bodies, 15 unchanged bystanders; working source raw-identical')
