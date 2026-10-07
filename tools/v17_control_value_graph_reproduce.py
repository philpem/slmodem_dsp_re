#!/usr/bin/env python3
"""Cross witnessed scale-value, owner-address and common-return graphs."""
import playbook_small_patterns as d


def variants(path, source):
    start, end, function = d.function(source, 'V17TX_control')
    cells = {}
    for compound in (False, True):
        for owner in (False, True):
            for common in (False, True):
                body = function
                if compound:
                    old = 'pps->cfg.scale = V17TX_PPS_SCALE[mode] * req->scale_mul;'
                    assert body.count(old) == 1
                    body = body.replace(old, 'pps->cfg.scale *= V17TX_PPS_SCALE[mode];')
                if owner:
                    assert body.count('\tstruct fpm_pps *pps;\n') == 1
                    assert body.count('\tpps = &block->pps;\n') == 1
                    body = body.replace('\tstruct fpm_pps *pps;\n', '').replace('\tpps = &block->pps;\n', '')
                    body = body.replace('pps->cfg.scale', 'block->pps.cfg.scale')
                if common:
                    old = '\tif (req->ctl1 & V17TXCTL_CTL1_BIT1) {\n\t\tV17TX_create(fp, fp);\n\t\treturn 1;\n\t}'
                    assert body.count(old) == 1
                    body = body.replace(old, '\tif (req->ctl1 & V17TXCTL_CTL1_BIT1)\n\t\tV17TX_create(fp, fp);')
                label = 'baseline' if not any((compound, owner, common)) else f'compound{int(compound)}-owner{int(owner)}-common{int(common)}'
                cells[label] = source[:start]+body+source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/V17t_stc.c',)
    d.OUT_NAME = 'v17-control-value-graph'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
