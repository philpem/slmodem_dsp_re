#!/usr/bin/env python3
"""Finite observed data-mode source-boundary controls."""
import playbook_small_patterns as d
d.REV='93d7eee1'
d.SOURCE_PATHS=('src/pump/v23/v23tx.c','src/pump/v32/V32.c')
d.OUT_NAME='batch20-data-states'
def variants(path, source):
    if path.endswith('/V32.c'):
        a,b,fn=d.function(source,'V32FP_create')
        start=fn.index('\tparams.protocol =')
        end=fn.index('\n\n\treturn ',start)
        statements=fn[start:end]
        options='\tparams.options = (params.options & ~0x400u)\n\t\t| (unsigned int)((cfg->r10 & 1) << 10);'
        assert options in statements
        lines={}
        for field in ('protocol','tx_rate','rx_rate','timeout','ec_near_delay','trellis','energy_drop_time'):
            lines[field]=next(x for x in statements.splitlines() if x.startswith('\tparams.'+field+' ='))
        cells={}
        for order in (False,True):
            for split in (False,True):
                body=statements
                if order:
                    body='\n'.join([lines['protocol'],lines['ec_near_delay'],options,lines['timeout'],lines['energy_drop_time'],lines['rx_rate'],lines['tx_rate'],lines['trellis']])
                if split: body=body.replace(options,'\tparams.options &= ~0x400u;\n\tparams.options |= (unsigned int)((cfg->r10 & 1) << 10);')
                label='baseline' if not(order or split) else ('stores' if order else '')+('-split' if split else '')
                cells[label]=source[:a]+fn[:start]+body+fn[end:]+source[b:]
        assert len(set(cells.values()))==4
        return cells
    cells={}
    a,b,fn=d.function(source,'v23FP_tx_create')
    old='''\ttx->period_index = 0;
\ttx->period_len = period_len;
\ttx->period = period;
\ttx->remaining = (unsigned short)period[0];
\ttx->resume = 0;
\ttx->mute = mute;
\ttx->space = space;
\ttx->mark = mark;'''
    new='''\ttx->period_len = period_len;
\ttx->space = space;
\ttx->period = period;
\ttx->mark = mark;
\ttx->period_index = 0;
\ttx->resume = 0;
\ttx->remaining = (unsigned short)period[0];
\ttx->mute = mute;'''
    assert fn.count(old)==1
    for store in (False,True):
        for cfg in (False,True):
            body=fn.replace(old,new) if store else fn
            if cfg:
                body=body.replace('\tcfg.scale = V23TX_SCALE;\n\tcfg.src = FPM_TONE_CFG.src;\t/* the shared 53-tap prototype */','\tcfg.src = FPM_TONE_CFG.src;\t/* the shared 53-tap prototype */\n\tcfg.scale = V23TX_SCALE;')
            label='baseline' if not(store or cfg) else ('stores' if store else '')+('-cfg' if cfg else '')
            cells[label]=source[:a]+body+source[b:]
    assert len(set(cells.values()))==4
    return cells
d.variants=variants
if __name__=='__main__': d.main()
