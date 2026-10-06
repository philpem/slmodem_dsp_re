#!/usr/bin/env python3
import playbook_small_patterns as d

def variants(path,source):
 shift='''\t\tfor (i = 0; i <= 14; i++)
\t\t\tcrc[i] = crc[i + 1];'''
 scalar='\n'.join('\t\tcrc[%d] = crc[%d];'%(i,i+1) for i in range(15))
 # Isolate helper only: the two independent unpackers have own untouched loops.
 hstart=source.index('static void\nv92jd_crc_bits(');hend=source.index('\n}\n',hstart)+2
 helper=source[hstart:hend];assert helper.count(shift)==1
 cells={'baseline':source,'expanded-pointer-helper':source[:hstart]+helper.replace(shift,scalar)+source[hend:]}
 for label,text in list(cells.items()):
  for method,vector in [('packJdData','bits'),('packJdPhaseData','phaseBits')]:
   a=text.index('V92Jd::'+method+'()');z=text.index('\n}\n',a)+2;fn=text[a:z]
   old='''\tfor (g = 0; g <= 1; g++)
\t\tv92jd_crc_bits(crc,
\t\t\t       &%s[V90JD_GROUP1 + 1 + g * V90JD_GROUP]);'''%vector
   assert fn.count(old)==1
   body='''\tfor (g = 0; g <= 1; g++) {
\t\tint k;
\t\tfor (k = 0; k <= 15; k++) {
\t\t\tint t = %s[V90JD_GROUP1 + 1 + g * V90JD_GROUP + k] + crc[0];
\t\t\tint tap3 = (crc[4] + t) & 1;
\t\t\tint tap10 = (crc[11] + t) & 1;
%s
\t\t\tcrc[3] = tap3;
\t\t\tcrc[10] = tap10;
\t\t\tcrc[15] = t & 1;
\t\t}
\t}'''%(vector, ('\n'.join('\t\t\tcrc[%d] = crc[%d];'%(i,i+1) for i in range(15)) if label!='baseline' else '\t\t\tfor (i = 0; i <= 14; i++)\n\t\t\t\tcrc[i] = crc[i + 1];'))
   text=text[:a]+fn.replace(old,body)+text[z:]
  # The file-static helper has no remaining caller and must not emit a new symbol.
  a=text.index('static void\nv92jd_crc_bits(');z=text.index('\n}\n',a)+2;text=text[:a]+text[z:]
  cells['direct-members-rolled' if label=='baseline' else 'direct-members-expanded']=text
 return cells
if __name__=='__main__':
 d.REV='6b4509bd';d.SOURCE_PATHS=('src/pump/v90/V92Jd.cpp',);d.OUT_NAME='batch-cpp-v92jd-crc';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
