#!/usr/bin/env python3
"""Test the original separate mu/low guards against accepted EchoCanceler.

One candidate, no profile palette. Predict distinct initial-RTL branches and
low surviving separately from its normalized count result through early CSE.
"""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import function_chunk
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data


def variants(path, source):
    a, z, fn = d.function(source, 'EchoCanceler')
    old = '''\t\tif (update) {
\t\t\tif (mu != 0.0f && low) {
\t\t\t\tfloat g = e * mu;
\t\t\t\tunsigned int m;

\t\t\t\tfor (m = 0; m < ntaps; m++)
\t\t\t\t\tcoef[m] += g * hist[pos + m];
\t\t\t}
\t\t}'''
    new = '''\t\tif (update) {
\t\t\tif (mu != 0.0f) {
\t\t\t\tif (low) {
\t\t\t\t\tfloat g = e * mu;
\t\t\t\t\tunsigned int m;

\t\t\t\t\tfor (m = 0; m < ntaps; m++)
\t\t\t\t\t\tcoef[m] += g * hist[pos + m];
\t\t\t\t}
\t\t\t}
\t\t}'''
    assert fn.count(old) == 1
    return {'baseline': source, 'nested-mu-low': source[:a]+fn.replace(old, new)+source[z:]}


if __name__ == '__main__':
    d.REV = '5d720ce7f'
    d.OUT_NAME = 'recovery-fdsp-nested-guard'
    d.SOURCE_PATHS = ('src/service/Fdspkrnl.c',)
    d.DUMP_FLAGS = ('-da',)
    d.variants = variants
    d.main()
    root = d.ROOT/'build'/d.OUT_NAME/'Fdspkrnl'
    before, after = root/'baseline/candidate.o', root/'nested-mu-low/candidate.o'
    old, new = Object(before).canonical(), Object(after).canonical()
    changed = [n for n in old[0] if old[0][n] != new[0][n]]
    assert set(old[0]) == set(new[0]) and set(changed) <= {'EchoCanceler'}
    assert old[1:] == new[1:]
    assert named_data(before) == named_data(after)
    assert {n:v for n,v in allocated(before).items() if n != '.text'} == {
        n:v for n,v in allocated(after).items() if n != '.text'}
    stages = {}
    for label in ('baseline', 'nested-mu-low'):
        stages[label] = {}
        for stage in ('01.rtl', '06.cse', '19.life', '20.combine'):
            chunk = function_chunk(root/label/('Fdspkrnl.c.'+stage), 'EchoCanceler')
            stages[label][stage] = {'integer_and_operations':chunk.count('(and:SI'),
                                     'low_name_occurrences':chunk.count('[ low ]'),
                                     'raw':chunk}
    assert stages['baseline']['01.rtl']['integer_and_operations'] == 1
    assert stages['nested-mu-low']['01.rtl']['integer_and_operations'] == 0
    assert stages['baseline']['20.combine']['low_name_occurrences'] == 0
    assert stages['nested-mu-low']['20.combine']['low_name_occurrences'] > 0
    report = {'cells':2, 'surviving_bodies':len(old[0]), 'changed_bodies':changed,
              'unchanged_bystanders':len(old[0])-len(changed),
              'data_bindings_consumers_equal':True, 'stages':stages,
              'raw_objects_equal':before.read_bytes() == after.read_bytes(),
              'controls':{'retained_AND_positive':True, 'nested_AND_absent':True,
                          'retained_low_removed_by_combine':True, 'nested_low_survives_combine':True},
              'next_discriminator':'Trace the original invariant e*mu product after the empty-tap guard versus the retained pre-guard product before proposing another source control.'}
    (root.parent/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('nested guard audit:', report['cells'], 'cells;',len(old[0]),
          'bodies; changed', changed, '; raw objects equal', report['raw_objects_equal'])
    print('integer AND by initial RTL:', {n:v['01.rtl']['integer_and_operations'] for n,v in stages.items()})
