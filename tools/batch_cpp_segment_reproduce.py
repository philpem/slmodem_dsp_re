#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 old='''\tfor (i = 0; i <= 7; i++) {
\t\tif ((int)level <=
\t\t    V90Phase3Modulator::codeSegmentsBoundriesLookupTable
\t\t\t[(int)m->pcmType][i])
\t\t\tbreak;
\t}'''
 new='''\ti = 0;
\tdo {
\t\tif ((int)level <=
\t\t    V90Phase3Modulator::codeSegmentsBoundriesLookupTable
\t\t\t[(int)m->pcmType][i])
\t\t\tbreak;
\t} while (++i <= 7);'''
 assert source.count(old)==1
 cells={'baseline':source,'helper-do':source.replace(old,new)}
 for label,text in list(cells.items()):
  a=text.index('static void\nupdateCodeSegment(');z=text.index('\n}\n',a)+3
  helper=text[a:z];body=helper[helper.index('{'):].replace('m->','')
  marker='''void
V90Phase3Modulator::updateCodeSegmentPointer()
{
\tupdateCodeSegment(this);
}'''
  assert text.count(marker)==1
  text=text[:a]+text[z:]
  text=text.replace(marker,'void\nV90Phase3Modulator::updateCodeSegmentPointer()\n'+body.rstrip())
  text=text.replace('updateCodeSegment(this);','updateCodeSegmentPointer();')
  cells['member-for' if label=='baseline' else 'member-do']=text
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',);d.OUT_NAME='batch-cpp-segment';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
