#!/usr/bin/env python3
import re
import playbook_small_patterns as d
d.REV='856c1ecb'
d.SOURCE_PATHS=tuple('src/pump/v90/'+x+'.cpp' for x in ['V90CP','V90MP','V92CP'])
d.OUT_NAME='gcc3-batch100-v90-crc-shift'
def variants(path,source):
 text=source; cls=path.rsplit('/',1)[-1][:-4]
 methods=['calcCRC'] if cls=='V92CP' else ['calcCRC','evaluateCRC']
 for method in methods:
  start,end,fn=d.function(text,cls+'::'+method)
  first=fn.index('\t\tcrc[0] = crc[1];');last=fn.index('\n',fn.index('\t\tcrc[15] =',first))
  old=fn[first:last];a='t' if cls=='V92CP' else 'a'
  tail=old[old.index('\t\tcrc[15] ='):]
  replacement='\t\tfor (k = 0; k < 15; k++)\n\t\t\tcrc[k] = crc[k + 1];\n\t\tcrc[3] = (unsigned char)((crc[3] + '+a+') & 1);\n\t\tcrc[10] = (unsigned char)((crc[10] + '+a+') & 1);\n'+tail
  fn=fn.replace('{\n','{\n\tint k;\n',1).replace(old,replacement,1)
  text=text[:start]+fn+text[end:]
 return {'baseline':source,'shift-then-taps':text}
d.variants=variants
if __name__=='__main__':d.main()
