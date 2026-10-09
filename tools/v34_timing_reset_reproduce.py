#!/usr/bin/env python3
"""Cross witnessed timing reset groups with the existing typed IIR history view."""
import playbook_small_patterns as d

ORDER = ('f1d4', 'f1e4', 'f1e8', 'timing_frac', 'timing_integrator', 'f1f0',
         'ppm_acc', 'ppm_count', 'timing_ppm', 'report_interval', 'pllcnt',
         'slow_ramp', 'mix_carrier_step', 'dwell_count', 'f22e', 'rx_blocks',
         'mix_carrier_phase', 'dp.iir2.q', 'dp.iir2.i', 'f208', 'f20a',
         'demod_q_prev', 'demod_i_prev')


def variants(path, source):
    a, z, fn = d.function(source, 'rxtiminginit')
    start = fn.index('\n\trx->rx_blocks = 0;')
    end = fn.rindex('\n}')
    block = fn[start:end]
    assignments = {}
    for line in block.splitlines():
        if line.startswith('\trx->'):
            field, value = line.strip().removeprefix('rx->').removesuffix(';').split(' = ')
            assignments[field] = value
    assert len(assignments) == 22 and assignments['dp.point'] == '0'
    cells = {}
    for order, halves in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        if order:
            values = dict(assignments)
            values['dp.iir2.i'] = values['dp.iir2.q'] = values.pop('dp.point')
            lines = []
            for field in ORDER:
                if not halves and field == 'dp.iir2.q':
                    continue
                dest = 'dp.point' if not halves and field == 'dp.iir2.i' else field
                lines.append('\trx->'+dest+' = '+values[field]+';')
            replacement = '\n'+'\n'.join(lines)+'\n'
        else:
            replacement = block
            if halves:
                replacement = replacement.replace('\trx->dp.point = 0;',
                                                  '\trx->dp.iir2.q = 0;\n\trx->dp.iir2.i = 0;')
        body = fn[:start]+replacement+fn[end:]
        cells['baseline' if not (order or halves) else f'order-{order}-halves-{halves}'] = source[:a]+body+source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '4bb720f3'
    d.OUT_NAME = 'v34-timing-reset'
    d.SOURCE_PATHS = ('src/pump/v34/V34RX.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
