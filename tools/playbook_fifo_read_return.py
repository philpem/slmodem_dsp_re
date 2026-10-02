#!/usr/bin/env python3
"""Three complete-TU FIFO8 read return controls with isolated public headers."""
import subprocess
import sys
import playbook_small_patterns as driver

REV = 'fcf0427a'
PATH = 'src/service/Fifo8.c'
HEADER = 'include/dsplib/fifo8.h'


def variants(path, source):
    start, end, fn = driver.function(source, 'FIFO8_read')
    assert source[start - 6:start] == 'short\n'
    assert fn.count('return (short)cnt;') == 1
    cells = {'baseline': source}
    for label, result in [('unsigned-short', 'unsigned short'), ('int', 'int')]:
        body = fn.replace('return (short)cnt;', 'return cnt;')
        cells[label] = source[:start - 6] + result + '\n' + body + source[end:]
    assert len(set(cells.values())) == 3
    return cells


def overlays(path, label):
    if label == 'baseline':
        return {}
    header = subprocess.check_output(
        ['git', 'show', REV + ':' + HEADER], cwd=driver.ROOT, text=True)
    assert header.count('short FIFO8_read(') == 1
    result = 'unsigned short' if label == 'unsigned-short' else 'int'
    return {'dsplib/fifo8.h': header.replace('short FIFO8_read(', result + ' FIFO8_read(')}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = REV
    driver.OUT_NAME = 'playbook-fifo-read-return'
    driver.SOURCE_PATHS = (PATH,)
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.main()
