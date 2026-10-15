#!/usr/bin/env python3
"""Cross original FSK combined phase, captured delayed tap and short counters."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'fskdetect')
    phase = '''\t\twork[i * V34_FSK_PHASES + 2] =
\t\t\t(short)interp_phase(intcoef3, hist);'''
    shift = '''\t\tfor (k = V34_FSK_TAPS; k > 0; k--)
\t\t\thist[k] = hist[k - 1];'''
    combined = '''\t\t{
\t\t\tunsigned acc = V34_FSK_ROUND;
\t\t\tfor (k = 0; k < V34_FSK_TAPS; k++) {
\t\t\t\tshort sample = hist[V34_FSK_TAPS - 1 - k];
\t\t\t\thist[V34_FSK_TAPS - k] = sample;
\t\t\t\tacc += (unsigned)(intcoef3[k] * sample);
\t\t\t}
\t\t\twork[i * V34_FSK_PHASES + 2] = (short)((int)acc >> 14);
\t\t}'''
    assert fn.count(phase) == fn.count(shift) == 1
    cells = {'baseline': source}
    for transfer, tap, word in itertools.product((False,True), repeat=3):
        if not (transfer or tap or word):
            continue
        text = fn
        if transfer:
            text = text.replace(phase, '').replace(shift, combined)
        if tap:
            text = text.replace('\t/* --- 2: delay and multiply --- */',
                                '\t/* --- 2: delay and multiply --- */\n\t{\n\tconst short *delayed = &d->line[cfg->delay];')
            text = text.replace('d->line[cfg->delay]', '*delayed')
            # Preserve the address expression in the new declaration.
            text = text.replace('const short *delayed = &*delayed;', 'const short *delayed = &d->line[cfg->delay];')
            text = text.replace('\t/* --- 3: low-pass, decimating 4:1 --- */', '\t}\n\n\t/* --- 3: low-pass, decimating 4:1 --- */')
        if word:
            assert text.count('\tint i, j, k;') == 1
            text = text.replace('\tint i, j, k;', '\tshort i, j, k;')
        full = source[:a]+text+source[z:]
        if word:
            h, q, helper = d.function(full, 'interp_phase')
            assert helper.count('\tint k;') == 1
            full = full[:h]+helper.replace('\tint k;', '\tshort k;')+full[q:]
        cells['transfer-%d-tap-%d-word-%d' % (transfer,tap,word)] = full
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '80da7b0c'
    d.OUT_NAME = 'gcc3-fskdetect-phase-delay'
    d.SOURCE_PATHS = ('src/pump/v34/DPSK.c',)
    d.variants = variants
    d.main()
