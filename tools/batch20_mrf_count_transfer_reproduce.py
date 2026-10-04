#!/usr/bin/env python3
"""Six original zero-extended MRF count handoffs, independent four-cell controls."""
import subprocess
import playbook_small_patterns as d
d.REV='93d7eee1';d.OUT_NAME='batch20-mrf-count-transfer'
functions={'src/pump/v32/V32int.c':'DemodDataV32','src/pump/b103/B103prc.c':'DemodDataB103','src/fax/V17r_int.c':'DemodDataV17','src/fax/V21r_int.c':'DemodDataV21','src/fax/V27r_int.c':'DemodDataV27','src/fax/V29r_int.c':'DemodDataV29'}
d.SOURCE_PATHS=tuple(functions)
def variants(path,source):
 a,b,fn=d.function(source,functions[path]);start=fn.index('FPM_MRF_filter(');end=fn.index(';',start);call=fn[start:end];assert call.count('(short)count')==1
 new=fn[:start]+call.replace('(short)count','count')+fn[end:];recovered=source[:a]+new+source[b:]
 return {'baseline':source,'count':recovered,'int':source,'int-count':recovered}
def overlays(path,label):
 if not label.startswith('int'):return {}
 s=subprocess.check_output(['git','show',d.REV+':include/dsplib/fpm_mrf.h'],cwd=d.ROOT,text=True);assert s.count('short count);')==1
 return {'dsplib/fpm_mrf.h':s.replace('short count);','int count);')}
d.variants=variants;d.HEADER_OVERLAYS=overlays
if __name__=='__main__':d.main()
