#!/usr/bin/env python3
"""Single witnessed signed-SAR control for call_run byte/sample count."""
import playbook_small_patterns as d

def variants(path, source):
    old = 'count -= n / 2;'
    assert source.count(old) == 1
    return {'baseline':source, 'signed-shift':source.replace(old, 'count -= n >> 1;')}

if __name__ == '__main__':
    d.REV='38afd5f7'
    d.SOURCE_PATHS=('src/call/call.c',)
    d.OUT_NAME='recovery-call-count-shift'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
