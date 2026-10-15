#!/usr/bin/env python3
"""Bound arithmetic captures while preserving final alias-visible sample reload."""
import playbook_small_patterns as d
import residual_tone_reversal_reproduce as earlier


def variants(path, source):
    sequential = earlier.variants(path, source)['sequential-1-sample-0']
    cells = {'baseline': source, 'sequential-repeat': sequential}
    for current, old in ((1, 0), (0, 1), (1, 1)):
        a, z, text = d.function(sequential, 'FPM_TONE_find_rev')
        marker = '\t\tcorr += ((samples[n] - hist[idx])'
        assert text.count(marker) == 1
        declarations = ''
        if current:
            declarations += '\t\tshort sample = samples[n];\n'
        if old:
            declarations += '\t\tshort outgoing = hist[idx];\n'
        text = text.replace(marker, declarations + '\n' + marker)
        start = text.index('\t\tcorr += ')
        end = text.index('\t\tif (2 * corr_s')
        arithmetic = text[start:end]
        if current:
            assert arithmetic.count('samples[n]') == 3
            arithmetic = arithmetic.replace('samples[n]', 'sample')
        if old:
            assert arithmetic.count('hist[idx]') == 3
            arithmetic = arithmetic.replace('hist[idx]', 'outgoing')
        text = text[:start] + arithmetic + text[end:]
        assert text.count('hist[idx] = samples[n];') == 1
        cells[f'current-{current}-outgoing-{old}'] = sequential[:a] + text + sequential[z:]
    assert len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    d.REV = '9c020b45'
    d.OUT_NAME = 'residual-tone-arithmetic'
    d.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
