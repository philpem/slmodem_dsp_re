#!/usr/bin/env python3
import gcc3_batch50_v90_adid_verdict as parent
d=parent.d
d.OUT_NAME='gcc3-batch50-v90-adid-cross'
def variants(path,source):
 oldcells=parent.variants(path,source);cells={'baseline':source,'sample-verdict':oldcells['sample-verdict']}
 for label,forms in [('sample-verdict',oldcells['sample-verdict']),('both-verdict',oldcells['phase-verdict-sample-verdict'])]:
  old='\t\tif (d > altRbsDistanceThresh)\n\t\t\tresult = 1;'
  assert forms.count(old)==1
  cells[label+'-complement']=forms.replace(old,'\t\tif (!(d <= altRbsDistanceThresh))\n\t\t\tresult = 1;')
  cells[label+'-reject-first']=forms.replace(old,'\t\tif (d <= altRbsDistanceThresh)\n\t\t\tresult = 0;\n\t\telse\n\t\t\tresult = 1;')
 return cells
d.variants=variants
if __name__=='__main__':d.main()
