#!/usr/bin/env python3
"""Predeclared terminal diagnostic exit overlays; no production edits."""
import playbook_small_patterns as d
d.REV='9f1199b57d3c91d1f26627eabd9eea496355798d'
d.SOURCE_PATHS=('src/pump/v90/V90Modem.cpp','src/pump/v90/V92Modem.cpp')
d.OUT_NAME='gcc3-mechanism-owner-screen-terminal'
def variants(path,source):
 cls=path.rsplit('/',1)[1].split('.')[0]
 start,end,fn=d.function(source,cls+'::reset')
 mark='\t\tbreak;\n\t}\n}'
 assert fn.count(mark)==1
 text=fn.replace(mark,'\t\treturn;\n\t}\n}')
 return {'baseline':source,'terminal-default-return':source[:start]+text+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
