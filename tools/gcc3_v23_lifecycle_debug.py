#!/usr/bin/env python3
"""Recover original diagnostics exposed by real V23 lifecycle transcripts."""
import playbook_small_patterns as d
from gcc3_v23_status_width import variants as width_variants, overlays


def recovered(path,text):
    if path.endswith('v23modem.c'):
        text=text.replace('\tm->reported = m->state;\n\n\tswitch (m->state)', '\tif (m->state != m->reported && DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("V23ModemMain: modem state moved from %d to %d\\n",\n\t\t\t\t     m->reported, m->state);\n\tm->reported = m->state;\n\n\tswitch (m->state)')
    elif path.endswith('bwchdem.c'):
        text=text.replace('#include "dsplib/sysdep.h"','#include "dsplib/debug.h"\n#include "dsplib/sysdep.h"')
        a='\t\t    == FPM_TONE_PRESENT)\n\t\t\tbw->carrier_blocks = (short)(bw->carrier_blocks + 1);\n\t\telse'
        b='\t\t    == FPM_TONE_PRESENT) {\n\t\t\tbw->carrier_blocks = (short)(bw->carrier_blocks + 1);\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23 backward channel: Tone detected 390[Hz]\\n");\n\t\t} else'
        assert text.count(a)==1;text=text.replace(a,b)
        a='\t\t\tbw->blocks = (short)(bw->blocks + 1);\n\t\t\treturn BWCH_GIVEN_UP;'
        b='\t\t\tbw->blocks = (short)(bw->blocks + 1);\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23Debug: Timeout waiting for V23 signal...\\r\\n");\n\t\t\treturn BWCH_GIVEN_UP;'
        assert text.count(a)==1;text=text.replace(a,b)
        a='\t\tif ((int)(unsigned short)bw->silence >= bw->silence_limit)\n\t\t\treturn BWCH_GIVEN_UP;'
        b='\t\tif ((int)(unsigned short)bw->silence >= bw->silence_limit) {\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("V23 no carrier detected...\\r\\n");\n\t\t\treturn BWCH_GIVEN_UP;\n\t\t}'
        assert text.count(a)==1;text=text.replace(a,b)
    else:
        assert path.endswith('v23rx.c')
        for field,message in [('acquire','Carrier Detection Time Out '),('silence','Energy drop detected......\\n')]:
            a='\t\tif (rx->'+field+'_limit <= rx->'+field+')\n\t\t\treturn V23RX_GIVEN_UP;'
            b='\t\tif (rx->'+field+'_limit <= rx->'+field+') {\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("'+message+'");\n\t\t\treturn V23RX_GIVEN_UP;\n\t\t}'
            assert text.count(a)==1;text=text.replace(a,b)
        a='\t\t\trx->rx_state = (short)(rx->rx_state\n\t\t\t\t\t       + V23RX_DETECT_STEP);'
        b=a+'\n\t\t\tif (DSPLIB_DEBUG_ON())\n\t\t\t\tdsplibs_debug_printf("v23 tone detected, counter = %d,threshold = 2\\n",\n\t\t\t\t\t(short)(rx->rx_state / V23RX_DETECT_STEP));'
        assert text.count(a)==1;text=text.replace(a,b)
    return text


def variants(path,source):
    text=width_variants(path,source)['both']
    return {'baseline':source,'all-diagnostics':recovered(path,text)}


def header(path,label):
    return overlays(path,'both') if label=='all-diagnostics' else {}


if __name__=='__main__':
    d.REV='861901c2'
    d.SOURCE_PATHS=('src/pump/v23/v23modem.c','src/pump/v23/bwchdem.c','src/pump/v23/v23rx.c')
    d.OUT_NAME='gcc3-v23-lifecycle-debug'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.HEADER_OVERLAYS=header
    d.main()
