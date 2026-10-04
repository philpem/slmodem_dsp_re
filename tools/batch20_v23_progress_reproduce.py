#!/usr/bin/env python3
"""Three original diagnostic sites independently crossed in each V23 receiver."""
import itertools
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23rx.c','src/pump/v23/bwchdem.c');d.OUT_NAME='batch20-v23-progress-literal-fixed'
def variants(path,source):
    a,b,fn=d.function(source,'v23FP_rx_progress' if path.endswith('/v23rx.c') else 'BwChDem_Progress')
    replacements=[]
    if path.endswith('/v23rx.c'):
        old='\t\tif (rx->acquire_limit <= rx->acquire)\n\t\t\treturn V23RX_GIVEN_UP;'
        new='\t\tif (rx->acquire_limit <= rx->acquire) {\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("Carrier Detection Time Out ");\n\t\t\treturn V23RX_GIVEN_UP;\n\t\t}'
        replacements.append((old,new))
        old='\t\t\trx->rx_state = (short)(rx->rx_state\n\t\t\t\t\t       + V23RX_DETECT_STEP);'
        new=old+'\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("v23 tone detected, counter = %d,threshold = 2\\n",\n\t\t\t\t\t\t     (short)(rx->rx_state / 5));'
        replacements.append((old,new))
        old='\t\tif (rx->silence_limit <= rx->silence)\n\t\t\treturn V23RX_GIVEN_UP;'
        new='\t\tif (rx->silence_limit <= rx->silence) {\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("Energy drop detected......\\n");\n\t\t\treturn V23RX_GIVEN_UP;\n\t\t}'
        replacements.append((old,new))
    else:
        old='\t\tif (FPM_TONE_detect(bw->tone, samples, count)\n\t\t    == FPM_TONE_PRESENT)\n\t\t\tbw->carrier_blocks = (short)(bw->carrier_blocks + 1);'
        new='\t\tif (FPM_TONE_detect(bw->tone, samples, count)\n\t\t    == FPM_TONE_PRESENT) {\n\t\t\tbw->carrier_blocks = (short)(bw->carrier_blocks + 1);\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23 backward channel: Tone detected 390[Hz]\\n");\n\t\t}'
        replacements.append((old,new))
        old='\t\t\tbw->blocks = (short)(bw->blocks + 1);\n\t\t\treturn BWCH_GIVEN_UP;'
        new='\t\t\tbw->blocks = (short)(bw->blocks + 1);\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23Debug: Timeout waiting for V23 signal...\\r\\n");\n\t\t\treturn BWCH_GIVEN_UP;'
        replacements.append((old,new))
        old='\t\tif ((int)(unsigned short)bw->silence >= bw->silence_limit)\n\t\t\treturn BWCH_GIVEN_UP;'
        new='\t\tif ((int)(unsigned short)bw->silence >= bw->silence_limit) {\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23 no carrier detected...\\r\\n");\n\t\t\treturn BWCH_GIVEN_UP;\n\t\t}'
        replacements.append((old,new))
    for old,new in replacements:assert fn.count(old)==1,(path,old)
    cells={}
    for axes in itertools.product((False,True),repeat=3):
        body=fn
        for on,(old,new) in zip(axes,replacements):
            if on:body=body.replace(old,new)
        text=source[:a]+body+source[b:]
        if any(axes):text=text.replace('#include "dsplib/sysdep.h"','#include "dsplib/sysdep.h"\n#include "dsplib/debug.h"')
        label='baseline' if not any(axes) else ''.join(str(int(x)) for x in axes)
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==8
    return cells
d.variants=variants
if __name__=='__main__':d.main()
