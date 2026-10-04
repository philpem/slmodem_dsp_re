#!/usr/bin/env python3
"""Full FloatARMA TU output-destination x feedback-acquisition controls."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 cells={}
 for dest in (0,1):
  for late in (0,1):
   text=source
   if dest:
    text=text.replace('static float\narma_convolve(const float *p, const float *c, unsigned int n)','static void\narma_convolve(float *result, const float *p, const float *c, unsigned int n)')
    text=text.replace('return (float)(odd + even);','*result = (float)(odd + even);')
    for result,args in [('m_fwd','x + xp, b, nB'),('m_fbk','y + yp, a, nA'),('m_fwd','x + xp, m_b, m_nB'),('m_fbk','y + yp, m_a, m_nA')]:
     old=result+' = arma_convolve('+args+');';assert text.count(old)==1
     text=text.replace(old,'arma_convolve(&'+result+', '+args+');')
   if late:
    a=text.index('\nFloatARMA::process(float in)')+1; z=text.index('\n}\n',a)+2; fn=text[a:z]
    fn=fn.replace('float *y = m_yhist;','float *y;').replace('int yp = m_ypos;','int yp;')
    marker='\n\t'+('arma_convolve(&m_fbk,' if dest else 'm_fbk = arma_convolve(')
    assert fn.count(marker)==1
    fn=fn.replace(marker,'\n\ty = m_yhist;\n\typ = m_ypos;'+marker)
    text=text[:a]+fn+text[z:]
   cells['baseline' if not(dest or late) else f'output-pointer-{dest}-late-feedback-{late}']=text
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-arma-output';driver.SOURCE_PATHS=('src/dsp/FloatARMA.cpp',);driver.variants=variants;driver.main()
