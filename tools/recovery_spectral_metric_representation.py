#!/usr/bin/env python3
"""Bounded scope/representation cross from original SF coefficient spills.

Only coefficient locals change; recurrence state and energy remain XF.
"""
import recovery_spectral_metric_lifetime as lifetime

d = lifetime.d
d.OUT_NAME = 'spectral-metric-representation'


def variants(path, source):
    cells = lifetime.variants(path, source)
    for label, text in list(cells.items()):
        start = text.index('V90SpectralShapingFilter::getMetric(')
        prefix, body = text[:start], text[start:]
        assert body.count('long double a0, a1, a2, a3;') == 1
        label = 'float-coefficients' if label == 'baseline' else 'per-block-float-coefficients'
        cells[label] = prefix + body.replace('long double a0, a1, a2, a3;', 'float a0, a1, a2, a3;')
    return cells


d.variants = variants
if __name__ == '__main__':
    d.main()
