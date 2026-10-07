#!/usr/bin/env python3
"""Replay known SDM controls to compare postreload forwarding, not search source.

Three cells: unchanged, existing mask-postincrement, existing compound-XOR
plus mask-postincrement. Both source properties were tested on byte-identical
FPM sibling in batch50. Current dumps provide causal before/after evidence.
"""
import playbook_small_patterns as d

def variants(path,source):
    a,b,fn=d.function(source,'SDM_descrambler')
    old='\t\t*data &= mask;\n\t\tdata++;'
    assert fn.count(old)==1
    post=fn.replace(old,'\t\t*data++ &= mask;')
    assignment='*data = (unsigned short)\n\t\t\t((reg >> shift1) ^ in ^ (reg >> shift2));'
    assert post.count(assignment)==1
    compound=post.replace(assignment,'*data ^= (reg >> shift1) ^ (reg >> shift2);')
    return {'baseline':source,'mask-postincrement':source[:a]+post+source[b:],
            'compound-postincrement':source[:a]+compound+source[b:]}

if __name__=='__main__':
    d.REV='fa941457';d.SOURCE_PATHS=('src/fax/SDM.c',);d.OUT_NAME='sdm-postreload'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
