#!/usr/bin/env python3
"""Ordinary staged allocation successes witnessed by original FDSP creator."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Fdsp.c',);d.OUT_NAME='batch100-fdsp-create-stage-flags'
def variants(path,s):
 a,z,f=d.function(s,'FDSP_DP_Create');out={}
 for stages,guarded in ((False,False),(True,False),(False,True),(True,True)):
  label='baseline'if not(stages or guarded)else'-'.join(x for yes,x in((stages,'distinct-stage-success'),(guarded,'guarded-one-success'))if yes);x=f
  if stages:
   x=x.replace('\t\tint ok;','\t\tint kernel_ok, buffers_ok = 0, chan_b_ok = 0;\n\t\tint chan_a_ok = 0, coef_b_ok = 0, coef_a_ok = 0;')
   x=x.replace('ok = k != 0;','kernel_ok = k != 0;')
   previous='kernel_ok';sequence=[('buffers','buffers_ok'),('chan_b','chan_b_ok'),('chan_a','chan_a_ok'),('chan_b->coef','coef_b_ok'),('chan_a->coef','coef_a_ok')]
   # First guarded group clears kernel's pointer fields, independently of allocation.
   x=x.replace('if (ok) {','if (kernel_ok) {',1)
   for field,flag in sequence:
    x=x.replace('if (ok) {','if ('+previous+') {',1)
    old='ok = k->'+field+' != 0;';new=flag+' = k->'+field+' != 0;';assert old in x;x=x.replace(old,new,1);previous=flag
   x=x.replace('if (!ok) {','if (!coef_a_ok) {')
  if guarded:
   for field,flag in [('buffers','buffers_ok'),('chan_b','chan_b_ok'),('chan_a','chan_a_ok'),('chan_b->coef','coef_b_ok'),('chan_a->coef','coef_a_ok')]:
    flag=flag if stages else 'ok';old='\t\t\t'+flag+' = k->'+field+' != 0;'
    new=(''if stages else'\t\t\t'+flag+' = 0;\n')+'\t\t\tif (k->'+field+' != 0)\n\t\t\t\t'+flag+' = 1;';assert old in x;x=x.replace(old,new,1)
  out[label]=s[:a]+x+s[z:]
 assert len(set(out.values()))==4
 return out
d.variants=variants
if __name__=='__main__':d.main()
