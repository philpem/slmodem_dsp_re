#!/usr/bin/env python3
"""Two full-TU predecessor conversion-order controls for RxClampV32."""
import playbook_small_patterns as driver


def variants(path, source):
    old='(short)(hdx->symbol_len - 1)';assert source.count(old)==1
    cells={'baseline':source,'convert-before-subtract':source.replace(old,'(short)hdx->symbol_len - 1')}
    assert len(cells)==len(set(cells.values()))==2
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-rxclamp-conversion'
    driver.SOURCE_PATHS=('src/pump/v32/V32int.c',)
    driver.variants=variants
    driver.main()
