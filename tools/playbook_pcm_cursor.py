#!/usr/bin/env python3
"""Two complete-TU PCM segment-table index/cursor controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'search_segment')
    assert fn.count('\tint seg;') == fn.count('seg_end[seg] >= mag') == 1
    changed = fn.replace('\tint seg;', '\tint seg;\n\tconst short *end = seg_end;')
    changed = changed.replace('seg_end[seg] >= mag', '*end++ >= mag')
    return {'baseline': source, 'cursor': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = '1b81dd2b'
    driver.OUT_NAME = 'playbook-pcm-cursor'
    driver.SOURCE_PATHS = ('src/service/pcm.c',)
    driver.variants = variants
    driver.main()
