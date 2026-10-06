#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 a=source.index('Sample\nGenericIIR<Sample, Coeff>::process(Sample x)');z=source.index('\n}\n',a)+2
 fn=source[a:z]
 member=fn.replace('long double acc = 0;','m_acc = 0;').replace('acc = acc +','m_acc = m_acc +').replace('acc = acc -','m_acc = m_acc -').replace('acc = acc /','m_acc = m_acc /').replace('\n\tm_acc = (Coeff)acc;\n','\n')
 assert ' acc' not in member[member.index('\tm_acc = 0;'):]
 captured=member.replace('if (m_outPos == 0)','if ((int)--m_outPos < 0)').replace('\n\telse\n\t\tm_outPos--;','').replace('if (m_inPos == 0)','if ((int)--m_inPos < 0)').replace('\n\telse\n\t\tm_inPos--;','')
 cells={'baseline':source,'member-acc':source[:a]+member+source[z:],'member-acc-countdown':source[:a]+captured+source[z:]}
 for label,text in list(cells.items()):
  text=text.replace('void\nGenericIIR<Sample, Coeff>::compactIn()','inline void\nGenericIIR<Sample, Coeff>::compactIn()').replace('void\nGenericIIR<Sample, Coeff>::compactOut()','inline void\nGenericIIR<Sample, Coeff>::compactOut()')
  cells[label+'-inline-compacts']=text
 text=cells['member-acc-inline-compacts']
 for field,nfield in [('In','nnum'),('Out','nden')]:
  old='''\tunsigned k;

\tm_%sPos = m_%sLen - m_%s;
\tfor (k = 0; k + 1 < m_%s; k++)
\t\tm_%sHist[m_%sLen - 1 - k] = m_%sHist[m_%s - 2 - k];'''%(field.lower(),field.lower(),nfield,nfield,field.lower(),field.lower(),field.lower(),nfield)
  new='''\tm_%sPos = m_%sLen - m_%s;
\tif (m_%s > 1) {
\t\tunsigned int count = m_%s - 1;
\t\tCoeff *src = m_%sHist + m_%s - 2;
\t\tCoeff *dst = m_%sHist + m_%sLen - 1;
\t\twhile (count-- != 0)
\t\t\t*dst-- = *src--;
\t}'''%(field.lower(),field.lower(),nfield,nfield,nfield,field.lower(),nfield,field.lower(),field.lower())
  assert text.count(old)==1; text=text.replace(old,new)
 cells['member-acc-inline-pointer-compacts']=text
 for label in ['baseline','baseline-inline-compacts','member-acc-inline-compacts','member-acc-inline-pointer-compacts']:
  text=cells[label]
  old='Sample\nGenericIIR<Sample, Coeff>::process(Sample x)'
  assert text.count(old)==1
  cells[label+'-inline-scalar']=text.replace(old,'inline Sample\nGenericIIR<Sample, Coeff>::process(Sample x)')
 for label in ['member-acc-countdown-inline-compacts','member-acc-inline-pointer-compacts-inline-scalar']:
  text=cells[label]
  if 'pointer' in label:
   text=text.replace('if (m_outPos == 0)','if ((int)--m_outPos < 0)').replace('\n\telse\n\t\tm_outPos--;','').replace('if (m_inPos == 0)','if ((int)--m_inPos < 0)').replace('\n\telse\n\t\tm_inPos--;','')
  cells[label+'-signed-cursors']=text.replace('(int)--m_outPos','--m_outPos').replace('(int)--m_inPos','--m_inPos')
 return cells

def overlays(path,label):
 if not label.endswith('-signed-cursors'):return {}
 text=(d.ROOT/'include/dsplib/GenericIIR.h').read_text()
 assert text.count('unsigned m_inPos;')==1 and text.count('unsigned m_outPos;')==1
 text=text.replace('unsigned m_inPos;','int m_inPos;').replace('unsigned m_outPos;','int m_outPos;')
 return {'dsplib/GenericIIR.h':text}
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/dsp/FloatIIR.cpp',);d.OUT_NAME='batch-cpp-iir';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.HEADER_OVERLAYS=overlays;d.main()
