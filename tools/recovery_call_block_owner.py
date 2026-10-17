#!/usr/bin/env python3
"""One original-witnessed inner dp->self owner reload control."""
import playbook_small_patterns as d

def variants(path, source):
    old = 'call_block(struct call_dp *st)\n{'
    assert source.count(old) == 1
    candidate = source.replace(old, 'call_block(struct dp *dp)\n{\n\tstruct call_dp *st = ((struct call_dp *)dp)->self;')
    assert candidate.count('call_block(st)') == 1
    return {'baseline':source, 'block-owner':candidate.replace('call_block(st)', 'call_block(dp)')}

if __name__ == '__main__':
    d.REV='ef46df76'
    d.SOURCE_PATHS=('src/call/call.c',)
    d.OUT_NAME='recovery-call-block-owner'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
