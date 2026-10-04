#!/usr/bin/env python3
"""Cross the observed quality value with independent byte/ABI boundaries."""
import subprocess
import playbook_small_patterns as d
import next20_count_predicate as pred

d.REV='8af3af53';d.OUT_NAME='next20-count-cross';d.DUMP_FLAGS=('-v','-save-temps','-da')
primary=('src/fax/V27r_int.c','src/fax/V27r_prc.c','src/fax/V29r_prc.c')
others=[str(p.relative_to(d.ROOT)) for p in sorted((d.ROOT/'src').glob('**/*.c')) if 'dsplib/v27fax.h' in p.read_text() and str(p.relative_to(d.ROOT)) not in primary]
d.SOURCE_PATHS=primary+tuple(others)
header=subprocess.check_output(['git','show',d.REV+':include/dsplib/v27fax.h'],cwd=d.ROOT,text=True)
old='void DescrambleDataV27(void *modem, unsigned short *data, short count);'
assert header.count(old)==1
candidate=header.replace(old,old.replace('short count','unsigned short count'))
d.HEADER_OVERLAYS=lambda path,label:{'dsplib/v27fax.h':candidate} if 'boundary-1' in label and path!='src/fax/V29r_prc.c' else {}

def variants(path,source):
 cells={}
 for mask in (False,True):
  for boundary in (False,True):
   label='baseline' if not(mask or boundary) else 'mask-%d-boundary-%d'%(mask,boundary)
   text=source
   if mask and path in primary[1:]:text=pred.variants(path,text)['mask-after-clear-1']
   if boundary and path==primary[0]:
    text=text.replace('DescrambleDataV27(void *modem, unsigned short *data, short count)','DescrambleDataV27(void *modem, unsigned short *data, unsigned short count)')
    a,z,fn=d.function(text,'DescrambleDataV27');fn=fn.replace('data, count);','data, (short)count);');text=text[:a]+fn+text[z:]
   if boundary and path==primary[2]:
    a,z,fn=d.function(text,'RxHdxDataV29')
    for flag in ('CARRIER','LOW_SNR'):
     fn=fn.replace('result.word |= V29_STATUS_'+flag,'result.byte.flags |= (unsigned char)(V29_STATUS_'+flag+' >> 8)')
     fn=fn.replace('result.word &= ~V29_STATUS_'+flag,'result.byte.flags &= (unsigned char)~(V29_STATUS_'+flag+' >> 8)')
    text=text[:a]+fn+text[z:]
   cells[label]=text
 return cells

d.variants=variants
if __name__=='__main__':d.main()
