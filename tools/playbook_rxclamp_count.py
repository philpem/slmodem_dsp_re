#!/usr/bin/env python3
"""Two complete-TU old-countdown controls for RxClampV32."""
import playbook_small_patterns as driver


def variants(path, source):
    old='\tfor (i = (short)(hdx->symbol_len - 1); i != -1; i--)\n\t\t*out++ = 0xff;'
    assert source.count(old)==1
    cells={'baseline':source,'postdecrement':source.replace(old,'\ti = (short)hdx->symbol_len;\n\twhile (i--)\n\t\t*out++ = 0xff;')}
    assert len(cells)==len(set(cells.values()))==2
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-rxclamp-count'
    driver.SOURCE_PATHS=('src/pump/v32/V32int.c',)
    driver.variants=variants
    driver.main()
