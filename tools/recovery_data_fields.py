#!/usr/bin/env python3
"""Replay unsigned V.22 RMS accumulation through every V.22 translation unit."""
from pathlib import Path
import subprocess
import playbook_small_patterns as driver

driver.REV = 'eccf0f2ae817cec09e93886ed7635604600a75dd'
driver.SOURCE_PATHS = tuple(str(p.relative_to(driver.ROOT)) for p in sorted((driver.ROOT/'src/pump/v22').glob('*.c')))
driver.OUT_NAME = 'recovery-data-fields'
driver.DUMP_FLAGS = ()


def header(path, label):
    if label == 'baseline':
        return {}
    text = subprocess.check_output(['git', 'show', driver.REV + ':include/dsplib/v22fp.h'], cwd=driver.ROOT, text=True)
    old = '\tint rms_accum;'
    assert text.count(old) == 1
    return {'dsplib/v22fp.h': text.replace(old, '\tunsigned int rms_accum;')}


def variants(path, source):
    candidate = source
    if path == 'src/pump/v22/v22mod.c':
        old = ('fp->hdx->rms_accum = (int)((unsigned int)fp->hdx->rms_accum\n'
               '\t\t\t\t\t     / (unsigned short)fp->hdx->rms_blocks);\n'
               '\t\t\tif ((unsigned int)fp->hdx->rms_accum')
        new = ('fp->hdx->rms_accum /= (unsigned short)fp->hdx->rms_blocks;\n'
               '\t\t\tif (fp->hdx->rms_accum')
        assert candidate.count(old) == 1
        candidate = candidate.replace(old, new)
    return {'baseline': source, 'unsigned-accumulator': candidate}


driver.HEADER_OVERLAYS = header
driver.variants = variants
if __name__ == '__main__':
    driver.main()
