#!/usr/bin/env python3
"""Separate observed epoch distance evaluations without changing narrowing."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'V29RX_epoch_det')
    old='\td = (short)(((di * di + dq * dq) >> 15)\n\t\t    + ((ei * ei + eq * eq) >> 15));'
    new='\td = (di * di + dq * dq) >> 15;\n\td = (short)(d + ((ei * ei + eq * eq) >> 15));'
    assert fn.count(old)==1
    separate=fn.replace(old,new)
    prior='\tei = (short)(dec->i2 - dec->i0);\n\teq = (short)(dec->q2 - dec->q0);\n'
    assert separate.count(prior)==1
    streamed=separate.replace(prior,'')
    split='\td = (di * di + dq * dq) >> 15;'
    streamed=streamed.replace(split,split+'\n'+prior.rstrip('\n'))
    return {'baseline':source,'separate-sums':source[:start]+separate+source[end:],'streamed-pairs':source[:start]+streamed+source[end:]}
if __name__=='__main__':
    d.REV='6b4509bd';d.SOURCE_PATHS=('src/fax/V29rx.c',);d.OUT_NAME='fax-epoch-pair-factoring'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
