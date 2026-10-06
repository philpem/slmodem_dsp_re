#!/usr/bin/env python3
"""Transfer constant member CRC shifts to untouched receive state machines."""
import itertools
import playbook_small_patterns as d


def variants(path, source):
    methods = ('V90Jd::unPackData',) if path.endswith('V90Jd.cpp') else (
        'V92Jd::unPackJdData', 'V92Jd::unPackJdPhaseData')
    old = '\t\t\tfor (i = 0; i <= 14; i++)\n\t\t\t\tcrc[i] = crc[i + 1];'
    expanded = '\n'.join('\t\t\tcrc[%d] = crc[%d];' % (i, i+1) for i in range(15))
    cells = {}
    for flags in itertools.product((False, True), repeat=len(methods)):
        text = source
        for method, enabled in zip(methods, flags):
            start, end, function = d.function(text, method)
            assert function.count(old) == 1, method
            if enabled:
                text = text[:start]+function.replace(old, expanded)+text[end:]
        label = 'baseline' if not any(flags) else 'expand-'+'-'.join(str(int(x)) for x in flags)
        cells[label] = text
    return cells


if __name__ == '__main__':
    d.REV = '548b5edd'
    d.SOURCE_PATHS = ('src/pump/v90/V90Jd.cpp', 'src/pump/v90/V92Jd.cpp')
    d.OUT_NAME = 'jd-unpack-fixed-shift'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
