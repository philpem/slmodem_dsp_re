#!/usr/bin/env python3
"""Three full-TU controls for fixed versus late-inlined segment bounds."""
import playbook_pcm_cursor as cursor


def variants(path, source):
    source = cursor.variants(path, source)['cursor']
    start, end, fn = cursor.driver.function(source, 'search_segment')
    assert fn.count('seg < 8') == fn.count('return 8;') == 1
    assert source.count('search_segment(mag)') == 2
    cells = {'baseline': source}
    for label, table_arg in [('size-argument', False), ('table-size-arguments', True)]:
        signature = 'search_segment(int mag, const short *table, int size)' if table_arg else 'search_segment(int mag, int size)'
        changed = fn.replace('search_segment(int mag)', signature)
        changed = changed.replace('seg < 8', 'seg < size').replace('return 8;', 'return size;')
        if table_arg:
            changed = changed.replace('const short *end = seg_end;', 'const short *end = table;')
        text = source[:start] + changed + source[end:]
        call = 'search_segment(mag, seg_end, 8)' if table_arg else 'search_segment(mag, 8)'
        cells[label] = text.replace('search_segment(mag)', call)
    assert len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    driver = cursor.driver
    driver.REV = '7ddff66c'
    driver.OUT_NAME = 'playbook-pcm-bound'
    driver.SOURCE_PATHS = ('src/service/pcm.c',)
    driver.variants = variants
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'pcm'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-pcm-cursor/pcm/cursor/candidate.o'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
