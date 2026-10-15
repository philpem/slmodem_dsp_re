#!/usr/bin/env python3
"""Cross observed sequential energy updates with current-sample capture."""
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'FPM_TONE_find_rev')
    cells = {'baseline': source}
    for sequential, sample in ((1, 0), (0, 1), (1, 1)):
        text = fn
        if sequential:
            old = '\t\tenergy += ((samples[n] * samples[n]) >> 15)\n\t\t\t  - ((hist[idx] * hist[idx]) >> 15);'
            assert text.count(old) == 1
            text = text.replace(old, '\t\tenergy += (samples[n] * samples[n]) >> 15;\n\t\tenergy -= (hist[idx] * hist[idx]) >> 15;')
        if sample:
            marker = '\t\tcorr += ((samples[n] - hist[idx])'
            assert text.count(marker) == 1
            text = text.replace(marker, '\t\tshort sample = samples[n];\n\n' + marker)
            # Retain the initial read; replace only later current-value uses.
            before, after = text.split('\t\tshort sample = samples[n];', 1)
            assert after.count('samples[n]') == 4
            text = before + '\t\tshort sample = samples[n];' + after.replace('samples[n]', 'sample')
        cells[f'sequential-{sequential}-sample-{sample}'] = source[:a] + text + source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '9c020b45'
    d.OUT_NAME = 'residual-tone-reversal'
    d.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
