#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V92Modem.cpp',);d.OUT_NAME='gcc3-batch100-v92modem-ctor-terminal'
def variants(path,source):
 start,end,fn=d.function(source,'V92Modem::V92Modem');mark='\t\tbreak;\n\t}\n}'
 assert fn.count(mark)==1;fn=fn.replace(mark,'\t\treturn;\n\t}\n}')
 return {'baseline':source,'terminal-illegal-side':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
