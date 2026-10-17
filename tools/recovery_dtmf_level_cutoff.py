#!/usr/bin/env python3
"""One original-operand and differential-backed DTMF cutoff value repair."""
import playbook_small_patterns as d

def variants(path,source):
    old='\tthreshold = (mode == DTMF_MODE_EUR) ? 0.0022f : 0.002f;'
    assert source.count(old)==1
    new='\t/* Original US cutoff is 0x3b03126e, one float below 0.002f. */\n\tthreshold = (mode == DTMF_MODE_EUR) ? 0.0022f : 0.0019999999f;'
    return {'baseline':source,'original-cutoff':source.replace(old,new)}

if __name__=='__main__':
    d.REV='79000775'
    d.OUT_NAME='recovery-dtmf-level-cutoff'
    d.SOURCE_PATHS=('src/service/Dtmf.c',)
    d.DUMP_FLAGS=()
    d.variants=variants
    d.main()
