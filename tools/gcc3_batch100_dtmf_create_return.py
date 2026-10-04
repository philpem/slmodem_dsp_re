#!/usr/bin/env python3
"""DTMF allocation failure retains the returned owner through its diagnostic."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-dtmf-create-return'
def variants(path,s):
 a,z,f=d.function(s,'create_dtmf')
 start=f.index('\tif (d == NULL) {');end=f.index('\n\tfor (i = 0;',start)
 f=f[:start]+'''\tif (d == NULL)
\t\td = sysdep_malloc(sizeof(struct dtmf));
\tif (d == NULL) {
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf(
\t\t\t    "could not allocate DTMF channel\\n");
\t} else {
'''+f[end:]
 f=f.replace('\treturn d;','\t}\n\treturn d;',1)
 return {'baseline':s,'failure-common-owner-return':s[:a]+f+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
