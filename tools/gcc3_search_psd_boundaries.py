#!/usr/bin/env python3
"""Compile the finite original-witness search/normalization boundary crosses."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    name = 'getConstellationsIndex' if path.endswith('V90MappingParamsInt.cpp') else 'Psd::process'
    a, z, fn = d.function(source, name)
    cells = {'baseline': source}
    for first, second in ((1, 0), (0, 1), (1, 1)):
        candidate = fn
        if name == 'getConstellationsIndex':
            if first:
                line = '\t\tunsigned int length = params->constellationSize[i];\n'
                assert candidate.count(line) == 1
                candidate = candidate.replace(line, '')
                entry = '\t\tfor (j = 0; j < count; j++) {\n'
                assert candidate.count(entry) == 1
                candidate = candidate.replace(entry, entry + '\t\t\tunsigned int length = params->constellationSize[i];\n')
            if second:
                old = '\t\t\tparams->distinctIndex[count] = (int)i;\n\t\t\tgroup[i] = (int)count;\n\t\t\tcount++;'
                assert candidate.count(old) == 1
                candidate = candidate.replace(old, '\t\t\tparams->distinctIndex[count++] = (int)i;\n\t\t\tgroup[i] = (int)(count - 1);')
        else:
            if first:
                candidate = candidate.replace('\t\tlong double scale;\n', '')
                candidate = candidate.replace('\t\tscale = 1.0L / peak;\n', '')
                candidate = candidate.replace('\t\tlong double scale = 1.0L / (long double)frames;\n', '')
                peak, rest = candidate.split('\tcase OUTPUT_LINEAR:', 1)
                peak = peak.replace('* scale + 1e-25', '/ peak + 1e-25')
                rest = rest.replace('* scale + 1e-25', '/ (long double)frames + 1e-25')
                rest = rest.replace('(long double)out[i] * scale', '(long double)out[i] / (long double)frames')
                candidate = peak + '\tcase OUTPUT_LINEAR:' + rest
                assert 'scale' not in candidate
            if second:
                assert candidate.count('long double peak = 0.0L;') == 1
                candidate = candidate.replace('long double peak = 0.0L;', 'float peak = 0.0f;')
        cells['axis-%d-%d' % (first, second)] = source[:a] + candidate + source[z:]
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd13949ad'
    d.OUT_NAME = 'gcc3-search-psd-boundaries'
    d.SOURCE_PATHS = ('src/pump/v90/V90MappingParamsInt.cpp', 'src/dsp/psd.cpp')
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
