#!/usr/bin/env python3
"""Authentic B103 constructor diagnostic crossed with recovered source/API."""
import playbook_small_patterns as d
import batch20_mrf_formal_reproduce as formal
import batch20_b103_state_reproduce as states
import batch20_b103_constructor_reproduce as banner
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/b103/B103prc.c',);d.OUT_NAME='batch20-b103-integrated-banner'
WIN='int-returns-owner-count'
def variants(path,source):
 recovered=formal.variants(path,source)[WIN];combined=states.variants(path,recovered)['org-ans'];debug=banner.variants(path,combined)['debug']
 return {'baseline':source,'combined':combined,'combined-debug':debug}
def overlays(path,label):return formal.overlays(path,'baseline' if label=='baseline' else WIN)
d.variants=variants;d.HEADER_OVERLAYS=overlays
if __name__=='__main__':d.main()
