#!/usr/bin/env python3
"""Cross the separately witnessed count acquisition with ACK preparation."""
import playbook_small_patterns as d
import residual_v22_ack_reproduce as earlier


def variants(path, source):
    combined = earlier.variants(path, source)['square-1-late-1']
    cells = {'baseline': source, 'combined-repeat': combined}
    for label, seed in (('count-after-ideal', source), ('combined-count-after-ideal', combined)):
        a, z, fn = d.function(seed, 'Detect_Rmloop2_ACK')
        old = '\tunsigned short n = *count;\n'
        assert fn.count(old) == 1
        fn = fn.replace(old, '')
        marker = '\tshort sumsq = 0;'
        assert fn.count(marker) == 1
        fn = fn.replace(marker, old + marker)
        cells[label] = seed[:a] + fn + seed[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'df4b23b9'
    d.OUT_NAME = 'residual-v22-ack-count'
    d.SOURCE_PATHS = ('src/pump/v22/v22prc.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
