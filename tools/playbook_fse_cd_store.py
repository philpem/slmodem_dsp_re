#!/usr/bin/env python3
"""Two staged CD successor/lms source-order controls after measured sched2 reversal."""
import playbook_small_patterns as driver
from playbook_fse_cd_conversion import variants as conversion_variants


def variants(path, source):
    parent = conversion_variants(path, source)['short-count-carrier-counter-use']
    start, end, fn = driver.function(parent, 'FSE_decision_CD')
    old = '\t\tstate->cfg.decision = FSE_decision_trn;\n\t\tstate->lms_on = 1;'
    new = '\t\tstate->lms_on = 1;\n\t\tstate->cfg.decision = FSE_decision_trn;'
    assert fn.count(old) == 1
    return {'baseline': parent, 'lms-before-successor': parent[:start] + fn.replace(old, new) + parent[end:]}


if __name__ == '__main__':
    driver.REV = 'b1632e79'
    driver.OUT_NAME = 'playbook-fse-cd-store'
    driver.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'V32dec'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-fse-cd-conversion/V32dec/short-count-carrier-counter-use/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
