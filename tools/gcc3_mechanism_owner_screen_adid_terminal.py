#!/usr/bin/env python3
"""Guard-local terminal return, with matched restricted RTL diagnostics."""
import playbook_small_patterns as d
d.REV='9f1199b57d3c91d1f26627eabd9eea496355798d'
d.SOURCE_PATHS=('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
d.OUT_NAME='gcc3-mechanism-owner-screen-adid-terminal'
d.DUMP_FLAGS=('-dr',)
def variants(path,source):
 start,end,fn=d.function(source,'V90AutoDigitalImpDetector::findPadGain')
 mark='\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("------------------------------------"\n\t\t\t\t     "------------------------------------" "-\\r\\n");\n}'
 assert fn.count(mark)==1
 text=fn.replace(mark,mark.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {').replace('\n}','\n\t\treturn;\n\t}\n}'))
 return {'baseline':source,'terminal-guard-return':source[:start]+text+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
