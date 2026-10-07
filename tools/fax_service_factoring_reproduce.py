#!/usr/bin/env python3
"""Original complete switch and common converter-result factoring."""
import playbook_small_patterns as d
import fax_service_values_reproduce as prior

def variants(path,source):
 cells={'baseline':source}
 for extra,explicit in [(False,True),(True,False),(True,True)]:
  base=prior.variants(path,source)['extra-1-mode-0'] if extra else source
  a,z,fn=d.function(base,'FAX_class1_command')
  if explicit:
   old='\tdefault:\t/* FAXC1_FRH */';assert fn.count(old)==1
   fn=fn.replace(old,'\tcase FAXC1_FRH:')
   tail='\t}\n\n\t(void)fax_class1_command'
   assert fn.count(tail)==1
   fn=fn.replace(tail,'\tdefault:\n\t\treturn -1;\n'+tail)
  cells[f'extra-{int(extra)}-sixcase-{int(explicit)}']=base[:a]+fn+base[z:]
 for mode,local in [(True,False),(False,True),(True,True)]:
  base=prior.variants(path,source)['extra-0-mode-1'] if mode else source
  a,z,fn=d.function(base,'FAX_create')
  if local:
   first='\tif (rate != 8000) {\n';assert fn.count(first)==1
   fn=fn.replace(first,first+'\t\tstruct rc *rc = NULL;\n\n')
   for field in ['rc_a','rc_b']:
    lo=fn.index('\t\tif (rate == 9600)',fn.index('struct rc *rc') if field=='rc_a' else fn.index('if (rc == NULL)'))
    hi=fn.index('\t\t\tgoto fail;',lo)+len('\t\t\tgoto fail;')
    old=fn[lo:hi]
    new=old.replace('ctx->'+field+' = RcFixed_Create','rc = RcFixed_Create')
    new=new.replace('\n\t\telse\n\t\t\tctx->'+field+' = NULL;','')
    new=new.replace('\n\t\tif (ctx->'+field+' == NULL)','\n\t\tctx->'+field+' = rc;\n\t\tif (rc == NULL)')
    if field=='rc_b':new='\t\trc = NULL;\n'+new
    fn=fn[:lo]+new+fn[hi:]
  cells[f'mode-{int(mode)}-converter-{int(local)}']=base[:a]+fn+base[z:]
 assert len(cells)==len(set(cells.values()))==7
 return cells
if __name__=='__main__':
 d.REV='a95a6c65';d.SOURCE_PATHS=('src/fax/fax.c',);d.OUT_NAME='fax-service-factoring'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
