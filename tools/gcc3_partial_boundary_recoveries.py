#!/usr/bin/env python3
"""Replay three independently witnessed partial boundaries in current context."""
import playbook_small_patterns as d
from fax_next_state_publication_reproduce import variants as v21
from fax_unframe_publication_reproduce import variants as unframe


def variants(path,source):
    if path.endswith('/V21r_prc.c'):
        old=v21(path,source)
        return {k:old[k] for k in ('baseline','post-diagnostic-owner')}
    if path.endswith('/faxvmi_hdlc.c'):
        return unframe(path,source)
    start,end,fn=d.function(source,'v23FP_rx_progress')
    old='FPM_MRF_filter(&rx->mrf, samples, samples, (short)count)'
    new='FPM_MRF_filter(&rx->mrf, samples, samples, (unsigned short)count)'
    assert fn.count(old)==1
    return {'baseline':source,'unsigned-MRF':source[:start]+fn.replace(old,new)+source[end:]}


if __name__=='__main__':
    d.REV='e8393850'
    d.SOURCE_PATHS=('src/pump/v23/v23rx.c','src/fax/V21r_prc.c','src/fax/faxvmi_hdlc.c')
    d.OUT_NAME='gcc3-partial-boundary-recoveries'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
