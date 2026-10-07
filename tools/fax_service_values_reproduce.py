#!/usr/bin/env python3
"""Original default dispatch argument and pre-callback config publication."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={}
 for extra,mode in itertools.product((False,True),repeat=2):
  text=source
  if extra:
   a,z,fn=d.function(text,'FAX_class1_command')
   old='\tint extra;';assert fn.count(old)==1
   text=text[:a]+fn.replace(old,'\tint extra = 0;')+text[z:]
  if mode:
   a,z,fn=d.function(text,'FAX_create')
   old='''\ts7 = modem_get_sreg(modem, 7);
\tlocal.mode = (originate != 0) ? CLASS1_ANS_ORG_NORMAL
\t\t\t\t      : CLASS1_ANS_ORG_ANSWER;'''
   new='''\tlocal.mode = (originate != 0) ? CLASS1_ANS_ORG_NORMAL
\t\t\t\t      : CLASS1_ANS_ORG_ANSWER;
\ts7 = modem_get_sreg(modem, 7);'''
   assert fn.count(old)==1;text=text[:a]+fn.replace(old,new)+text[z:]
  label=('extra-'+str(int(extra))+'-mode-'+str(int(mode))) if extra or mode else 'baseline'
  cells[label]=text
 assert len(cells)==len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='a95a6c65';d.SOURCE_PATHS=('src/fax/fax.c',);d.OUT_NAME='fax-service-values'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
