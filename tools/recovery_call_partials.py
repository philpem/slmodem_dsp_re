#!/usr/bin/env python3
"""Replay the fixed CID/Tone partial graphs against eccf0f2a full TUs."""
import sys
from pathlib import Path

import playbook_small_patterns as driver
from recovery_cid_terminal_audit import inspect


def variants(path, source):
    candidate = (driver.ROOT / path).read_text()
    cells = {'baseline': source}
    if path.endswith('toneiir.c'):
        before, body = candidate.split('\n_iir_filter_progress(', 1)
        body = body.replace('\n\tshort *shift = f->shift;', '').replace('shift[', 'f->shift[')
        cells['expanded-short'] = before + '\n_iir_filter_progress(' + body
    cells['partial'] = candidate
    return cells


if __name__ == '__main__':
    driver.REV = 'eccf0f2a'
    driver.OUT_NAME = 'recovery-fields-call'
    driver.SOURCE_PATHS = ('src/service/Cidfsd.c', 'src/callprog/toneiir.c')
    driver.variants = variants
    driver.main()
    import json
    from elftools.elf.elffile import ELFFile
    out = driver.ROOT / 'build' / driver.OUT_NAME
    results = json.loads((out / 'results.json').read_text())
    audited = 0
    for family, record in results['families'].items():
        baseline = out / family / 'baseline/candidate.o'
        for label, cell in record['cells'].items():
            assert inspect(out / family / label / 'candidate.o') == inspect(baseline)
            assert not cell.get('gains') and not cell.get('losses')
            audited += 1
    baseline = out / 'Cidfsd/baseline/candidate.o'
    raw = bytearray(baseline.read_bytes())
    with baseline.open('rb') as stream:
        offset = ELFFile(stream).get_section_by_name('.data')['sh_offset']
    raw[offset] ^= 1
    negative = out / 'data-negative.o'
    negative.write_bytes(raw)
    assert inspect(negative) != inspect(baseline)
    print('%d/%d complete TU data/binding/metadata audits passed; data corruption detected 1/1.' % (audited, audited))
