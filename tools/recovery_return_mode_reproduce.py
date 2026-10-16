#!/usr/bin/env python3
"""Eight full-TU local-result controls from the first-use mode screen."""
import playbook_small_patterns as driver
from recovery_sre_boundary import REV

PATHS = ('src/fax/V21t_prc.c', 'src/fax/V27t_int.c', 'src/fax/V27t_prc.c',
         'src/pump/b103/B103prc.c')


def variants(path, source):
    names = {'V21t_prc.c': 'TxHdxStartV21', 'V27t_int.c': 'TxNoCarrierV27',
             'V27t_prc.c': 'TxHdxDataV27', 'B103prc.c': 'DemodDataB103'}
    start, end, body = driver.function(source, names[path.rsplit('/', 1)[-1]])
    if path.endswith('V21t_prc.c'):
        changes = [('short nsamples;', 'unsigned short nsamples;'),
                   ('nsamples = (short)ModDataV21(', 'nsamples = (unsigned short)ModDataV21(')]
    elif path.endswith('V27t_int.c'):
        changes = [('short r;', 'unsigned short r;'),
                   ('r = (short)FPM_PPS_filter(', 'r = (unsigned short)FPM_PPS_filter(')]
    elif path.endswith('V27t_prc.c'):
        changes = [('short got;', 'unsigned short got;'),
                   ('got = (short)FIFO_read(', 'got = (unsigned short)FIFO_read(')]
    else:
        changes = [('int nbits = 0;', 'unsigned short nbits = 0;')]
    candidate = body
    for old, new in changes:
        assert candidate.count(old) == 1, (path, old)
        candidate = candidate.replace(old, new)
    return {'baseline': source, 'unsigned-local': source[:start]+candidate+source[end:]}


if __name__ == '__main__':
    driver.REV = REV
    driver.OUT_NAME = 'recovery-return-mode-local'
    driver.SOURCE_PATHS = PATHS
    driver.variants = variants
    driver.DUMP_FLAGS = ()
    driver.main()
