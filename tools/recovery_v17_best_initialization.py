#!/usr/bin/env python3
"""Measure the documented extra V.17 slicer initialization on the period TU.

Prediction: reproducing the native uninitialised best local removes its extra
zero store. This does not establish reachability or equivalence of junk values.
The shipping build retains its existing fallback through the bug-reproduction
switch. Other source, flags and full-TU exports must remain controlled.
"""
import playbook_small_patterns as driver
import json
import re
from recovery_fax_debug_audit import Object


def variants(path, source):
    old = '\tshort k, base = 0, best = 0;'
    assert source.count(old) == 1
    replacement = '''\tshort k, base = 0, best;
#ifndef DSPLIB_REPRODUCE_BUGS
\tbest = 0;
#endif'''
    return {'baseline': source, 'native-best': source.replace(old, replacement)}


if __name__ == '__main__':
    driver.REV = '96ce7c20d'
    driver.SOURCE_PATHS = ('src/fax/V17rxdec.c',)
    driver.OUT_NAME = 'recovery-v17-best'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    driver.variants = variants
    driver.main()
    folder = driver.ROOT / 'build' / driver.OUT_NAME / 'V17rxdec'
    baseline = Object(folder / 'baseline/candidate.o').canonical()
    candidate = Object(folder / 'native-best/candidate.o').canonical()
    assert baseline[1:] == candidate[1:], 'data, binding or relocation consumer drift'
    changed = [name for name in baseline[0]
               if baseline[0][name] != candidate[0][name]]
    assert changed == ['FAX_FSE_decision_128pt'], changed
    # The unchanged baseline is the positive control for the extra assignment.
    # Initial RTL names the local before later allocation obscures its origin.
    initializers = {}
    for label in ('baseline', 'native-best'):
        rtl = (folder / label / 'V17rxdec.c.01.rtl').read_text()
        rtl = rtl.split(';; Function FAX_FSE_decision_128pt\n', 1)[1].split(
            ';; Function ', 1)[0]
        initializers[label] = len(re.findall(
            r'\(set \(reg/v:SI \d+ \[ best \]\)\s+\(const_int 0 ', rtl))
    assert initializers == {'baseline': 1, 'native-best': 0}, initializers
    report = {'shared_bodies': len(baseline[0]), 'changed_bodies': changed,
              'data_binding_relocation_consumers_unchanged': True,
              'initial_rtl_best_zero_assignments': initializers,
              'limits': 'No assertion of exceptional-path reachability or junk-value equivalence.'}
    (folder / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, sort_keys=True))
