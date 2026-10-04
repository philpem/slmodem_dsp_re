#!/usr/bin/env python3
"""Two original unsigned MRF handoffs under fixed adopted consistent API."""
import subprocess
import playbook_small_patterns as d
import batch20_v23_rx_ratio_reproduce as rx
d.REV='93d7eee1';d.SOURCE_PATHS=('src/service/Rxcid.c','src/pump/v23/v23rx.c');d.OUT_NAME='batch20-mrf-last-handoffs'
def replace(path,source):
 name='cid_modem' if path.endswith('/Rxcid.c') else 'v23FP_rx_progress'
 a,b,fn=d.function(source,name)
 if name=='cid_modem':
  old='FPM_MRF_filter(&cid->mrf, buf, buf, ncount)';new='FPM_MRF_filter(&cid->mrf, buf, buf, count)'
 else:
  old='FPM_MRF_filter(&rx->mrf, samples, samples, (short)count)';new='FPM_MRF_filter(&rx->mrf, samples, samples, (unsigned short)count)'
 assert fn.count(old)==1;return source[:a]+fn.replace(old,new)+source[b:]
def variants(path,source):
 cells={'baseline':source,'unsigned':replace(path,source)}
 if path.endswith('/v23rx.c'):
  combined=rx.variants(path,source)['boundary-ratio'];cells.update({'combined':combined,'combined-unsigned':replace(path,combined)})
 return cells
def overlays(path,label):
 s=subprocess.check_output(['git','show',d.REV+':include/dsplib/fpm_mrf.h'],cwd=d.ROOT,text=True);assert s.count('short count);')==1
 return {'dsplib/fpm_mrf.h':s.replace('short count);','int count);')}
d.HEADER_OVERLAYS=overlays;d.variants=variants
if __name__=='__main__':d.main()
