#!/usr/bin/env python3
"""Cross pulse owner acquisition and original input versus corrected count."""
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 start,end,fn=d.function(source,'PulseDialDigit')
 for owner in (False,True):
  for count in (False,True):
   if not owner and not count: continue
   body=fn
   if owner:
    body=body.replace('struct call *c = call_of(modem);','struct call *c = call_of(modem);\n\tstruct call *st;')
    body=body.replace('\t/* Zero dials ten,','\tst = c->self;\n\n\t/* Zero dials ten,')
    body=body.replace('c->self->pulse_remaining','st->pulse_remaining').replace('c->self->pulse_elapsed','st->pulse_elapsed')
   if count:
    body=body.replace('struct call *c = call_of(modem);','int remaining;\n\tstruct call *c = call_of(modem);')
    body=body.replace('if (digit == 0)\n\t\tdigit = 10;','remaining = digit;\n\tif (digit == 0)\n\t\tremaining = 10;')
    body=body.replace('pulse_remaining = digit;','pulse_remaining = remaining;').replace('MDMPRM_PULSE_DIAL, digit);','MDMPRM_PULSE_DIAL, remaining);')
   cells['owner-%d-count-%d'%(owner,count)]=source[:start]+body+source[end:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/call/call.c',);d.OUT_NAME='gcc3-batch50-pulse-digit';d.variants=variants;d.main()
