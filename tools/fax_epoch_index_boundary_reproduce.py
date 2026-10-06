#!/usr/bin/env python3
"""Cross the observed post-counter sample-index read with pair evaluation."""
import playbook_small_patterns as d
import fax_epoch_pair_factoring_reproduce as pairs

def late_index(text):
    start,end,fn=d.function(text,'V29RX_epoch_det')
    old='\tshort n = (short)state->n_out;';assert fn.count(old)==1
    fn=fn.replace(old,'\tshort n;')
    marker='\ti = state->out_i[n];';assert fn.count(marker)==1
    fn=fn.replace(marker,'\tn = (short)state->n_out;\n'+marker)
    return text[:start]+fn+text[end:]

def variants(path,source):
    stream=pairs.variants(path,source)['streamed-pairs']
    return {'baseline':source,'late-index':late_index(source),'streamed-control':stream,'streamed-late-index':late_index(stream)}
if __name__=='__main__':
    d.REV='6b4509bd';d.SOURCE_PATHS=('src/fax/V29rx.c',);d.OUT_NAME='fax-epoch-index-boundary'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
