#!/usr/bin/env python3
"""Cross owner lifetime and observed result/count-extension boundaries."""
import itertools
import playbook_small_patterns as driver

def variants(path,source):
 cells={}
 for reload,result,implicit in itertools.product((False,True),repeat=3):
  label='baseline' if not any((reload,result,implicit)) else '-'.join(word for enabled,word in ((reload,'reload'),(result,'unsigned-result'),(implicit,'implicit-count')) if enabled)
  text=source
  for name in ('ModDataB103','TxNoCarrierB103'):
   start,end,fn=driver.function(text,name)
   if reload:
    old='\tstruct b103_dsp *dsp = fp->dsp;\n';assert fn.count(old)==1
    fn=fn.replace(old,'').replace('dsp->','fp->dsp->')
   if result:
    assert text[start-6:start]=='short\n'
    text=text[:start-6]+'unsigned short\n'+text[start:];start+=9;end+=9
    assert fn.count('return (short)(unsigned short)')==1
    fn=fn.replace('return (short)(unsigned short)','return (unsigned short)')
   if implicit:
    assert fn.count('(short)nsamples')==1
    fn=fn.replace('(short)nsamples','nsamples')
   text=text[:start]+fn+text[end:]
  cells[label]=text
 assert len(cells)==len(set(cells.values()))==8
 return cells

def overlays(path,label):
 if 'unsigned-result' not in label:return {}
 rel='dsplib/b103fp.h';text=(driver.ROOT/'include'/rel).read_text()
 for name in ('ModDataB103','TxNoCarrierB103'):
  old='short '+name+'(';assert text.count(old)==1
  text=text.replace(old,'unsigned short '+name+'(')
 return {rel:text}

if __name__=='__main__':
 driver.REV='6cde9a9d'
 driver.OUT_NAME='playbook-b103-tx-width'
 driver.SOURCE_PATHS=('src/pump/b103/B103prc.c',)
 driver.variants=variants
 driver.HEADER_OVERLAYS=overlays
 driver.main()
