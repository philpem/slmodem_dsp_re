#!/usr/bin/env python3
"""All observed MRF/FSD caller and callee consumers, baseline/restoration."""
import playbook_small_patterns as driver
import playbook_free_arguments as recovery


def variants(path,source):
    cells=recovery.variants(path,source)
    return {label:cells[label] for label in ('baseline','both')}


if __name__ == '__main__':
    driver.REV='97c06e4e'
    driver.OUT_NAME='playbook-free-arguments-all'
    driver.SOURCE_PATHS=recovery.SOURCE_PATHS + ('src/pump/b103/B103prc.c','src/pump/v32/V32.c','src/fax/V17rx.c','src/fax/V21rx.c','src/fax/V21tx.c','src/fax/V27rx.c','src/fax/V29rx.c')
    driver.variants=variants
    driver.HEADER_OVERLAYS=recovery.overlays
    driver.main()
