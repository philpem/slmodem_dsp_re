#!/usr/bin/env python3
"""Two binary-witnessed pointer/publication lifetime controls in full fax TUs."""
import playbook_small_patterns as d

def variants(path,source):
    if path.endswith('/SDM.c'):
        start,end,fn=d.function(source,'SDM_descrambler')
        old='\t\t*data &= mask;\n\t\tdata++;'
        assert fn.count(old)==1
        fn=fn.replace(old,'\t\t*data++ &= mask;')
        return {'baseline':source,'mask-postincrement':source[:start]+fn+source[end:]}
    start,end,fn=d.function(source,'FSE_decision_eqtrn')
    tail='\tpt = DECv17_MAP_TRN[c];\n\t*angle = DECv17_ANGL4800[pt];\n\t*mag = FSE_HANDSHAKE_MAG;\n\treturn (unsigned short)pt;'
    assert fn.count(tail)==1
    fn=fn.replace('\n'+tail,'')
    branch_tail='\n'+''.join('\t'+line+'\n' for line in tail.splitlines())
    first='\t\tm->scram = (int)((sr << 2) | (unsigned int)c);'
    last='\t\tm->int_0050 = 0;'
    assert fn.count(first)==fn.count(last)==1
    fn=fn.replace(first,first+branch_tail).replace(last,last+branch_tail)
    return {'baseline':source,'branch-local-publication':source[:start]+fn+source[end:]}
if __name__=='__main__':
    d.REV='fa941457';d.SOURCE_PATHS=('src/fax/SDM.c','src/fax/V17rxdec.c');d.OUT_NAME='fax-tail-timing'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
