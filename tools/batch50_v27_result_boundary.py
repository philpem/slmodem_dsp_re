#!/usr/bin/env python3
"""V27 unsigned result lifetime versus signed return and API crossing."""
import batch50_v27_descramble_api as api
import playbook_small_patterns as d
d.SOURCE_PATHS=('src/fax/V27r_int.c','src/fax/V27r_prc.c');d.OUT_NAME='batch50-v27-result-boundary'
d.HEADER_OVERLAYS=lambda path,label: {'dsplib/v27fax.h':api.candidate} if label.startswith('unsigned-api') else {}
def variants(path,source):
 cells={'baseline':source}
 for result,formal,label in [(True,False,'result-width'),(False,True,'unsigned-api'),(True,True,'unsigned-api-result')]:
  text=source
  if path.endswith('V27r_int.c') and formal:
   text=text.replace('DescrambleDataV27(void *modem, unsigned short *data, short count)','DescrambleDataV27(void *modem, unsigned short *data, unsigned short count)')
   a,z,fn=d.function(text,'DescrambleDataV27');fn=fn.replace('data, count);','data, (short)count);');text=text[:a]+fn+text[z:]
  if path.endswith('V27r_prc.c') and result:
   a,z,fn=d.function(text,'RxHdxDataV27');assert '\tshort r;' in fn
   fn=fn.replace('\tshort r;','\tunsigned short r;').replace('r = (short)(QualityDetectV27(modem) != V27_QUALITY_UNRELIABLE ? n : 0);','r = (QualityDetectV27(modem) != V27_QUALITY_UNRELIABLE ? n : 0);').replace('\treturn r;','\treturn (short)r;')
   text=text[:a]+fn+text[z:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
