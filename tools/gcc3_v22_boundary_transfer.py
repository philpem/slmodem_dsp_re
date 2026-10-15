#!/usr/bin/env python3
"""Review the original V22 full-slot count and word-at-use boundary."""
import playbook_small_patterns as d
import batch20_v22_mrf_formal_reproduce as prior

if __name__=='__main__':
    d.REV='ec575202'
    d.SOURCE_PATHS=('src/pump/v22/V22int.c','src/pump/v22/v22_mrf.c')
    d.OUT_NAME='gcc3-v22-boundary-transfer'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=prior.variants
    d.HEADER_OVERLAYS=prior.overlays
    d.main()
