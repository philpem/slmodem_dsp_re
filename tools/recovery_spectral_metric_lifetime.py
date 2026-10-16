#!/usr/bin/env python3
"""Cross the observed per-block getMetric coefficient lifetime with retained source."""
import playbook_small_patterns as d

d.REV = 'b856fde4'
d.OUT_NAME = 'spectral-metric-lifetime'
d.SOURCE_PATHS = ('src/pump/v90/V90SpectralShapingFilter.cpp',)
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')


def variants(path, source):
    start = source.index('V90SpectralShapingFilter::getMetric(')
    head, body = source[:start], source[start:]
    declaration = '\tlong double a0, a1, a2, a3;\n'
    loads = '\ta0 = coeff[0];\n\ta1 = coeff[1];\n\ta2 = coeff[2];\n\ta3 = coeff[3];\n'
    anchor = '\t\tunsigned int n = blockLength;\n'
    assert body.count(declaration) == body.count(loads) == body.count(anchor) == 1
    candidate = body.replace(declaration, '').replace(loads, '')
    candidate = candidate.replace(anchor, anchor + '\t\tlong double a0, a1, a2, a3;\n\n' +
                                  '\n'.join('\t' + line for line in loads.rstrip().split('\n')) + '\n')
    return {'baseline': source, 'per-block-coefficients': head + candidate}


d.variants = variants
if __name__ == '__main__':
    d.main()
