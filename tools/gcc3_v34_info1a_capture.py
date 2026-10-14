#!/usr/bin/env python3
"""Cross independently witnessed INFO1a cache and pointer-capture boundaries."""
import playbook_small_patterns as d


def variants(path, source):
    start,end,fn=d.function(source,'V34GiveINFO1aBits')
    old=('\t*((char *)session_ptr(sess, SESSION_CAPS) + 8) = (char)(short)uinfo;\n'
         '\t*((char *)session_ptr(sess, SESSION_UPSTREAM) + 8) =\n'
         '\t\t(char)(short)uinfo;')
    new=('\t{\n\t\tchar *upstream = (char *)session_ptr(sess, SESSION_UPSTREAM);\n'
         '\t\tchar *caps = (char *)session_ptr(sess, SESSION_CAPS);\n\n'
         '\t\tcaps[8] = (char)(short)uinfo;\n'
         '\t\tupstream[8] = (char)(short)uinfo;\n\t}')
    assert fn.count(old)==1 and fn.count('\tint uinfo;')==1
    cells={}
    for cache in ('retained','short','int-narrow'):
        for pointers in (False,True):
            label='baseline' if cache=='retained' and not pointers else cache+('-pointers' if pointers else '')
            body=fn
            if cache=='short':
                body=body.replace('\tint uinfo;','\tshort uinfo;')
            elif cache=='int-narrow':
                assert body.count('uinfo = bitreverse(')==1
                body=body.replace('uinfo = bitreverse(','uinfo = (short)bitreverse(')
            if pointers:
                body=body.replace(old,new)
            cells[label]=source[:start]+body+source[end:]
    assert len(set(cells.values()))==6
    return cells


if __name__=='__main__':
    d.REV='1ec83de9'
    d.SOURCE_PATHS=('src/pump/v34/v34info.c',)
    d.OUT_NAME='gcc3-v34-info1a-capture'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
