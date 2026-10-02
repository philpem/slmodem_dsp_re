#!/usr/bin/env python3
"""Cross the reference state-free guard and repeated child-owner evaluation."""
import sys
import playbook_small_patterns as driver
from playbook_fixedrc_factory import variants as factory


def variants(path, source):
    parent = factory(path, source)['factory-contract']
    cells = {'baseline': source, 'factory-signed': parent}
    for label, guarded, reload in [('state-guard', True, False),
                                  ('child-reloads', False, True),
                                  ('guard-and-reloads', True, True)]:
        start, end, fn = driver.function(parent, 'RcFixed_Delete')
        if reload:
            declaration = '\t\tstruct rc_kind1 *k1 = (struct rc_kind1 *)h->state;\n'
            assert fn.count(declaration) == 1
            fn = fn.replace(declaration, '')
            for field in ('w6','w174','w30'):
                fn = fn.replace('k1->'+field, '((struct rc_kind1 *)h->state)->'+field)
        if guarded:
            old = '\tif (h->state != NULL && h->kind == 1) {'
            assert fn.count(old) == 1
            fn = fn.replace(old, '\tif (h->state != NULL) {\n\tif (h->kind == 1) {')
            old = '\tsysdep_free(h->state);\n'
            assert fn.count(old) == 1
            fn = fn.replace(old, old+'\t}\n')
        cells[label] = parent[:start] + fn + parent[end:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '53c3bd00'
    driver.OUT_NAME = 'playbook-fixedrc-delete-lifetime'
    driver.SOURCE_PATHS = ('src/core/FixedRC.c',)
    driver.variants = variants
    driver.main()
