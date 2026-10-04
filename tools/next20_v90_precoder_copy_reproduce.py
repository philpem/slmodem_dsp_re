#!/usr/bin/env python3
"""Fixed-block copies in the original V92 precoder reset."""
import playbook_small_patterns as d

def variants(path,source):
    a,z,fn=d.function(source,'V92Precoder::reset')
    # The first overload takes no arguments; select the configuration overload.
    a=source.index('\nV92Precoder::reset(V92MappingParams *params)')+1
    z=source.index('\n}\n',a)+2;fn=source[a:z]
    old='''\tfor (i = 0; i < 6; i++) {
\t\thead[i] = (unsigned int *)p->constellations[i];
\t\ttableB[i] = (int)p->LC[i];
\t}

\tfor (i = 0; i < 12; i++)
\t\ttableA[i] = p->m[i];'''
    new='''\tmemcpy(tableB, p->LC, sizeof(tableB));
\tmemcpy(head, p->constellations, sizeof(head));
\tmemcpy(tableA, p->m, sizeof(tableA));'''
    assert fn.count(old)==1
    fn=fn.replace(old,new).replace('\tint i;\n','')
    candidate=source[:a]+fn+source[z:]
    assert candidate.count('#include <stddef.h>')==1
    candidate=candidate.replace('#include <stddef.h>','#include <stddef.h>\n#include <string.h>')
    return {'baseline':source,'fixed-block-copies':candidate}
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-precoder-copy';d.SOURCE_PATHS=('src/pump/v90/V92Precoder.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
