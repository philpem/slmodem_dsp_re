#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 a=source.index('int\nV90SdDetector::process(');z=source.index('\n}\n',a)+2;fn=source[a:z]
 old='''\tdo {
\t\thistory[i] = history[i - 1];
\t} while (--i != 0);'''
 new='''\tfloat *src = history + historyLength - 2;
\tfloat *dst = history + historyLength - 1;
\tdo {
\t\t*dst-- = *src--;
\t} while (--i != 0);'''
 assert fn.count(old)==1
 forms={'baseline':fn,'pointer-tail':fn.replace(old,new)}
 for label,text in list(forms.items()):
  text=text.replace('\tunsigned int i = historyLength - 1;','\tint result = 0;\n\tunsigned int i = historyLength - 1;',1)
  text=text.replace('\t\treturn run < limit ? 0 : 1;','\t\tif (run >= limit)\n\t\t\tresult = 1;\n\t\treturn result;',1)
  forms['captured-result' if label=='baseline' else 'pointer-captured']=text
 text=forms['pointer-captured']
 start=text.index('\tif (thresh_08 > energy) {')
 text=text[:start]+'''\tif (!(thresh_08 > energy)) {
\t\tratio = correlation / energy;
\t\tif (!(thresh_0c >= ratio)) {
\t\t\trun = count + 1;
\t\t\tcount = run;
\t\t\tif (run >= limit)
\t\t\t\tresult = 1;
\t\t} else if (value_10 > ratio) {
\t\t\tresult = -1;
\t\t} else {
\t\t\tcount = 0;
\t\t}
\t} else {
\t\tcount = 0;
\t}
\treturn result;
}'''
 forms['pointer-common-result']=text
 return {label:source[:a]+f+source[z:] for label,f in forms.items()}
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v90/V90SdDetector.cpp',);d.OUT_NAME='batch-cpp-sd';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
