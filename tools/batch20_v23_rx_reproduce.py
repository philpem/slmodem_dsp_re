#!/usr/bin/env python3
"""V23 receiver: original diagnostic crossed with observed config lifetimes."""
import itertools
import playbook_small_patterns as d
import batch20_v23_debug_reproduce as debug
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23rx.c',);d.OUT_NAME='batch20-v23-rx'
def variants(path,source):
    old='\tstruct fpm_mrf_cfg mrf;\n\tstruct fpm_fsd_cfg fsd;\n\tstruct fpm_tone_cfg tone;'
    decls=['\tstruct fpm_mrf_cfg mrf;','\tstruct fpm_fsd_cfg fsd;','\tstruct fpm_tone_cfg tone;']
    tone='\ttone.freq = V23RX_TONE_HZ;\n\ttone.ratio = V23RX_TONE_RATIO;\n\ttone.src = FPM_TONE_CFG.src;\t/* the shared 53-tap prototype */'
    member='\trx->rx_state = 0;\n\trx->iir_coeff = V23_IIR_FILT;'
    assert all(source.count(x)==1 for x in (old,tone,member))
    cells={}
    debug_source=debug.variants(path,source)['debug']
    for order in itertools.permutations(range(3)):
        for diag,tone_order,member_order in itertools.product((False,True),repeat=3):
            body=debug_source if diag else source
            body=body.replace(old,'\n'.join(decls[x] for x in order))
            if tone_order: body=body.replace(tone,'\ttone.src = FPM_TONE_CFG.src;\t/* the shared 53-tap prototype */\n\ttone.ratio = V23RX_TONE_RATIO;\n\ttone.freq = V23RX_TONE_HZ;')
            if member_order: body=body.replace(member,'\trx->iir_coeff = V23_IIR_FILT;\n\trx->rx_state = 0;')
            label='baseline' if order==(0,1,2) and not(diag or tone_order or member_order) else ''.join(str(x) for x in order)+('-debug' if diag else '')+('-tone' if tone_order else '')+('-member' if member_order else '')
            cells[label]=body
    assert len(cells)==len(set(cells.values()))==48
    return cells
d.variants=variants
if __name__=='__main__':d.main()
