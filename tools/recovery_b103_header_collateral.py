#!/usr/bin/env python3
"""Eight complete-TU controls for the B103 return header's other consumers."""
import playbook_small_patterns as driver
import recovery_b103_return_slots as slots
from recovery_sre_boundary import REV

def variants(path,source):
    return {'baseline':source,'r1_header':source}

if __name__=='__main__':
    driver.REV=REV
    driver.OUT_NAME='recovery-b103-header-collateral'
    driver.SOURCE_PATHS=('src/core/dp_init.c','src/pump/b103/b103_cfg.c',
                         'src/pump/b103/b103_tables.c','src/pump/b103/b103.c')
    driver.variants=variants
    driver.HEADER_OVERLAYS=slots.overlays
    driver.DUMP_FLAGS=()
    driver.main()
