#!/usr/bin/env python3
"""Two full-TU controls for linear2alaw's input/magnitude carrier."""
import re
import playbook_pcm_bound as bounds


def variants(path, source):
    source = bounds.variants(path, source)['size-argument']
    driver = bounds.cursor.driver
    start, end, fn = driver.function(source, 'linear2alaw')
    assert fn.count('int mask, seg, mag;') == fn.count('mag = pcm_val;') == 1
    assert fn.count('mag = -pcm_val - 8;') == 1
    changed = fn.replace('int mask, seg, mag;', 'int mask, seg;')
    changed = changed.replace('\t\tmag = pcm_val;\n', '')
    changed = changed.replace('mag = -pcm_val - 8;', 'pcm_val = -pcm_val - 8;')
    changed = re.sub(r'\bmag\b', 'pcm_val', changed)
    return {'baseline': source, 'input-magnitude': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver = bounds.cursor.driver
    driver.REV = '1b81dd2b'
    driver.OUT_NAME = 'playbook-pcm-magnitude'
    driver.SOURCE_PATHS = ('src/service/pcm.c',)
    driver.variants = variants
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'pcm'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-pcm-bound/pcm/size-argument/candidate.o'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
