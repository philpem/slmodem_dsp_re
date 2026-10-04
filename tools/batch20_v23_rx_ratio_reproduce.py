#!/usr/bin/env python3
"""Cross measured V23 receiver source boundaries with its literal blob ratio."""
import playbook_small_patterns as d
import batch20_v23_rx_reproduce as rx
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v23/v23rx.c',);d.OUT_NAME='batch20-v23-rx-ratio'
def variants(path,source):
    source_family=rx.variants(path,source)
    winner=source_family['201-debug-tone']
    old='#define V23RX_TONE_RATIO\t28998';new='#define V23RX_TONE_RATIO\t29000'
    assert source.count(old)==1
    return {'baseline':source,'ratio':source.replace(old,new),'boundary':winner,'boundary-ratio':winner.replace(old,new)}
d.variants=variants
if __name__=='__main__':d.main()
