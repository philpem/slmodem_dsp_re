#!/usr/bin/env python3
"""Direct owner-member arguments at initial V27 deletion calls."""
import playbook_small_patterns as driver


def variants(path,source):
    text=source
    if path.endswith('V27rx.c'):
        for family,member in (('FSE','fse'),('SRE','sre'),('MRF','mrf')):
            old='\trx = ((struct v27_rx *)modem)->rx;\n\tFPM_'+family+'_free((&rx->'+member+'), 1);'
            new='\tFPM_'+family+'_free(&((struct v27_rx *)modem)->rx->'+member+', 1);'
            assert text.count(old)==1
            text=text.replace(old,new)
    else:
        old='\ttx = ((struct v27_tx *)modem)->tx;\n\tFPM_PPS_free(&tx->pps, 1);'
        new='\tFPM_PPS_free(&((struct v27_tx *)modem)->tx->pps, 1);'
        assert text.count(old)==1
        text=text.replace(old,new)
    assert text!=source
    return {'baseline':source,'direct':text}


if __name__=='__main__':
    driver.REV='d3abfa0b'
    driver.OUT_NAME='playbook-v27-delete-arguments'
    driver.SOURCE_PATHS=('src/fax/V27rx.c','src/fax/V27tx.c')
    driver.variants=variants
    driver.main()
