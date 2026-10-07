#!/usr/bin/env python3
"""Transfer independently observed scale and request-flag value ages."""
import playbook_small_patterns as d


def variants(path, source):
    v27 = path.endswith('V27t_stc.c')
    target = 'V27TX_control' if v27 else 'V29TX_control'
    start, end, function = d.function(source, target)
    cells = {}
    for scale in (False, True):
        for flags in (False, True):
            body = function
            if v27:
                if scale:
                    body = body.replace('struct fpm_pps *pps;', 'struct v27_tx_block *block;')
                    old = '\tpps = (struct fpm_pps *)(void *)\n\t\t&((struct v27_tx *)modem)->tx->pps;'
                    assert body.count(old) == 1
                    body = body.replace(old, '\tblock = ((struct v27_tx *)modem)->tx;')
                    old = '\tpps->cfg.scale = ctl->scale_mul *\n\t\tV27TX_PPS_SCALE[rate];'
                    assert body.count(old) == 1
                    body = body.replace(old, '\tblock->pps.cfg.scale = ctl->scale_mul;\n\tint scaled = block->pps.cfg.scale * V27TX_PPS_SCALE[rate];')
                    old = '\t((struct v27_tx *)modem)->cfg.int_0018 = ctl->int_0010;'
                    assert body.count(old) == 1
                    body = body.replace(old, old+'\n\tblock->pps.cfg.scale = scaled;')
                if flags:
                    body = body.replace('\tflags = ctl->flags;\n', '')
                    body = body.replace('if (flags & V27TXCTL_FLAGS_FORCE_INT_0008)', 'if (ctl->flags & V27TXCTL_FLAGS_FORCE_INT_0008)')
                    body = body.replace('if (flags & V27TXCTL_FLAGS_REINIT)', 'if (ctl->flags & V27TXCTL_FLAGS_REINIT)')
            else:
                if scale:
                    old = '\t((struct v29_tx_root *)fp)->tx->pps.cfg.scale = V29TX_PPS_SCALE[rate] * req->scale_mul;'
                    assert body.count(old) == 1
                    body = body.replace(old, '\t((struct v29_tx_root *)fp)->tx->pps.cfg.scale *= V29TX_PPS_SCALE[rate];')
                if flags:
                    old = '\tprm->int_0008 =\n\t\t(req->ctl1 & V29TXCTL_CTL1_BIT4) != 0;'
                    assert body.count(old) == 1
                    body = body.replace(old, '\tunsigned char flags = req->ctl1;\n\tint enabled = 0;\n\tif (flags & V29TXCTL_CTL1_BIT4)\n\t\tenabled = 1;\n\tprm->int_0008 = enabled;')
                    body = body.replace('if (req->ctl1 & V29TXCTL_CTL1_BIT1)', 'if (flags & V29TXCTL_CTL1_BIT1)')
            label = 'baseline' if not(scale or flags) else f'scale{int(scale)}-flags{int(flags)}'
            cells[label] = source[:start]+body+source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/V27t_stc.c', 'src/fax/V29t_stc.c')
    d.OUT_NAME = 'fax-control-age-transfer'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
