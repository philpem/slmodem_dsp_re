#!/usr/bin/env python3
"""Bound the silence mean's field/local ownership across the debug call.

Prediction: the original's two threshold comparisons arise because its debug
return reloads the published energy, while its no-debug arm keeps that value
in x87.  A field comparison should recover this split; direct-field updates
test whether the original accumulator was also expressed through the owner.
Four complete-TU cells, with no production flags changed.
"""
import re
import sys
import playbook_small_patterns as d


def variants(path, source):
    old = '\t\tif (acc > silence_level_table[lvl]) {'
    assert source.count(old) == 1
    compared = source.replace(old, '\t\tif (s->energy > silence_level_table[lvl]) {')
    old_debug = '(int)(acc * SILENCE_FULLSCALE2),'
    assert compared.count(old_debug) == 1
    published = compared.replace(old_debug, '(int)(s->energy * SILENCE_FULLSCALE2),')
    direct = source.replace('\tfloat acc;\n', '')
    direct = direct.replace('\tacc = s->energy;\n', '')
    direct = direct.replace('\t\t\ts->energy = acc;\n', '')
    direct = direct.replace('\t\ts->energy = acc;\n', '')
    direct = re.sub(r'\bacc\b', 's->energy', direct)
    assert 'float acc' not in direct and 's->energy = s->energy;' not in direct
    cells = {'baseline': source, 'compare-field': compared,
             'published-field': published, 'direct-field': direct}
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '239b234c0'
    d.SOURCE_PATHS = ('src/service/silence.c',)
    d.OUT_NAME = 'recovery-silence-energy'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fdump-tree-original')
    if '--precision' in sys.argv:
        sys.argv.remove('--precision')
        original_variants = variants
        def precision_variants(path, source):
            direct = original_variants(path, source)['direct-field']
            old = '(float)s->nsamp < SILENCE_BLOCK'
            assert direct.count(old) == 1
            cells = {'baseline': source}
            for conversion in ('float', 'double'):
                for literal in ('800.0f', '800.0'):
                    cells[conversion+'-'+literal] = direct.replace(
                        old, '('+conversion+')s->nsamp < '+literal)
            cells['native-double'] = direct.replace(
                old, 's->nsamp < SILENCE_BLOCK').replace(
                '#define SILENCE_BLOCK\t\t800.0f', '#define SILENCE_BLOCK\t\t800.0')
            assert len(set(cells.values())) == 6
            return cells
        d.OUT_NAME = 'recovery-silence-precision'
        d.variants = precision_variants
    else:
        d.variants = variants
    d.main()
