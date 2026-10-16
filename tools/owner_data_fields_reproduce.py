#!/usr/bin/env python3
"""Replay the witnessed V.32 low-word SNR counter layout on full TUs."""
import subprocess
from pathlib import Path
import playbook_small_patterns as driver

driver.REV = '1d4b891e'
driver.SOURCE_PATHS = tuple(str(p.relative_to(driver.ROOT)) for p in sorted((driver.ROOT/'src/pump/v32').glob('*.c')))
driver.OUT_NAME = 'owner-data-fields'
driver.DUMP_FLAGS = ()

def header(path, label):
    if label == 'baseline': return {}
    text = subprocess.check_output(['git', 'show', driver.REV + ':include/dsplib/v32fp.h'], cwd=driver.ROOT, text=True)
    old = '\tint r20;\t\t/* +0x20 template 0                          */'
    new = '\tshort snr_drops;\t/* +0x20 signed SNR-drop counter             */\n\tshort short_22;\t\t/* +0x22 copied, no individual access seen   */'
    assert text.count(old) == 1
    text = text.replace(old, new).replace('short r2c;', 'short snr_settle_blocks;')
    return {'dsplib/v32fp.h': text}

def variants(path, source):
    text = source
    if path.endswith('/V32stc.c'):
        text = text.replace('(short)((unsigned short *)&modem->params.r2c)[0]', 'modem->params.snr_settle_blocks')
        text = text.replace('((unsigned short *)&modem->params.r2c)[0]', 'modem->params.snr_settle_blocks')
        text = text.replace('(short)((unsigned short *)&modem->params.r20)[0]', 'modem->params.snr_drops')
        text = text.replace('((unsigned short *)&modem->params.r20)[0]', 'modem->params.snr_drops')
    if path.endswith('/V32.c'):
        text = text.replace('p->r2c', 'p->snr_settle_blocks')
        text = text.replace('/* r20                                       */', '/* snr_drops                                 */\n\t0,\t\t\t/* short_22                                  */')
        text = text.replace('/* r2c                                       */', '/* snr_settle_blocks                         */')
    return {'baseline': source, 'typed-counters': text}

driver.HEADER_OVERLAYS = header
driver.variants = variants
if __name__ == '__main__': driver.main()
