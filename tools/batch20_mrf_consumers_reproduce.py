#!/usr/bin/env python3
"""All MRF consumers: consistent int formal and combined diagnostic gains."""
import playbook_small_patterns as d
import batch20_mrf_formal_reproduce as formal
import batch20_b103_state_reproduce as states
import batch20_v23_rx_ratio_reproduce as rx
d.REV='93d7eee1'
d.SOURCE_PATHS=('src/dsp/fpm_mrf.c','src/pump/b103/B103prc.c','src/pump/v32/V32int.c','src/pump/v23/v23rx.c','src/service/Rxcid.c','src/fax/V17r_int.c','src/fax/V21r_int.c','src/fax/V21t_int.c','src/fax/V27r_int.c','src/fax/V29r_int.c')
d.OUT_NAME='batch20-mrf-consumers-validated'
WINNER='int-returns-owner-count'
def variants(path,source):
    if path in ('src/dsp/fpm_mrf.c','src/pump/b103/B103prc.c'):
        full=formal.variants(path,source);cells={'baseline':full['baseline'],'winner':full[WINNER]}
    else:cells={'baseline':source,'winner':source}
    combined=cells['winner']
    if path.endswith('/B103prc.c'):combined=states.variants(path,combined)['org-ans']
    if path.endswith('/v23rx.c'):combined=rx.variants(path,combined)['boundary-ratio']
    cells['combined']=combined
    return cells
def overlays(path,label):return formal.overlays(path,WINNER if label in ('winner','combined') else 'baseline')
d.HEADER_OVERLAYS=overlays;d.variants=variants
if __name__=='__main__':d.main()
