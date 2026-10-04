#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/dsp/FloatARMA.cpp',);d.OUT_NAME='gcc3-batch100-floatarma-count'
def variants(path,source):
 cells={'baseline':source}
 for count,result,label in [(True,False,'postdecrement'),(False,True,'result-pointer-control'),(True,True,'postdecrement-result-pointer')]:
  text=source
  if result:
   old='static float\narma_convolve(const float *p, const float *c, unsigned int n)';assert text.count(old)==1
   text=text.replace(old,'static void\narma_convolve(float *result, const float *p, const float *c, unsigned int n)')
   old='return (float)(odd + even);';assert text.count(old)==1;text=text.replace(old,'*result = (float)(odd + even);')
   for member,args in [('m_fwd','x + xp, b, nB'),('m_fbk','y + yp, a, nA'),('m_fwd','x + xp, m_b, m_nB'),('m_fbk','y + yp, m_a, m_nA')]:
    old=member+' = arma_convolve('+args+');';assert text.count(old)==1;text=text.replace(old,'arma_convolve(&'+member+', '+args+');')
  if count:
   start,end,fn=d.function(text,'FloatARMA::process');assert fn.count('\tdo {')==1 and fn.count('} while (--count != 0);')==1
   fn=fn.replace('\tdo {','\twhile (count-- != 0) {').replace('} while (--count != 0);','}');text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
