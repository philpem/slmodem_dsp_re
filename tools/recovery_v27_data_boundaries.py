#!/usr/bin/env python3
"""Cross four witnessed V27 DATA boundaries and two consistent wrapper formals."""
from itertools import product
import subprocess
import playbook_small_patterns as driver
from recovery_sre_boundary import REV

HEADER = 'include/dsplib/v27fax.h'


def variants(path, source):
    if path.endswith('V27t_int.c'):
        old = 'ScrambleDataV27(void *modem, unsigned short *data, short count)'
        assert source.count(old) == source.count('\t\t\t data, count);') == 1
        return {'baseline': source,
                'f1_wrapper': source.replace(old, old.replace('short count)', 'int count)'))
                .replace('\t\t\t data, count);', '\t\t\t data, (short)count);')}
    start, end, body = driver.function(source, 'TxHdxDataV27')
    cells = {}
    for formal, words, owner, switch in product((0, 1), repeat=4):
        label = 'baseline' if not any((formal, words, owner, switch)) else \
            'f%d_w%d_o%d_s%d' % (formal, words, owner, switch)
        text = body
        if words:
            for variable in ('taken', 'got', 'r'):
                assert text.count('short '+variable+';') == 1
                text = text.replace('short '+variable+';', 'unsigned short '+variable+';')
            text = text.replace('got = (short)FIFO_read(', 'got = (unsigned short)FIFO_read(')
            assert text.count('r = (short)ModDataV27(') == 3
            text = text.replace('r = (short)ModDataV27(', 'r = (unsigned short)ModDataV27(')
            for old, new in [('*budget <= got', '(unsigned short)*budget <= got'),
                             ('(short)(*budget - got)', '(short)((unsigned short)*budget - got)'),
                             ('(short)(*budget - taken)', '(short)((unsigned short)*budget - taken)')]:
                assert text.count(old) >= 1
                text = text.replace(old, new)
        if owner:
            assert text.count('if (prm->int_0008 != 0)') == 1
            text = text.replace('if (prm->int_0008 != 0)',
                                'if (((struct v27_tx *)modem)->source->int_0008 != 0)')
        if switch:
            old = '''\t\tif (rate == 0)
\t\t\t((struct v27_tx *)modem)->result.byte.status =
\t\t\t\tV27TX_STATUS_ENTER_DATA_2400;
\t\telse if (rate == 1)
\t\t\t((struct v27_tx *)modem)->result.byte.status =
\t\t\t\tV27TX_STATUS_ENTER_DATA_4800;
\t\telse
\t\t\t((struct v27_tx *)modem)->result.byte.status =
\t\t\t\tV27TX_STATUS_DEFAULT;'''
            new = '''\t\tswitch (rate) {
\t\tcase 0:
\t\t\t((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_ENTER_DATA_2400;
\t\t\tbreak;
\t\tcase 1:
\t\t\t((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_ENTER_DATA_4800;
\t\t\tbreak;
\t\tdefault:
\t\t\t((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_DEFAULT;
\t\t\tbreak;
\t\t}'''
            assert text.count(old) == 1
            text = text.replace(old, new)
        cells[label] = source[:start]+text+source[end:]
    return cells


def overlays(path, label):
    if not label.startswith('f1_'):
        return {}
    text = subprocess.check_output(['git', 'show', REV+':'+HEADER], cwd=driver.ROOT, text=True)
    old = 'void ScrambleDataV27(void *modem, unsigned short *data, short count);'
    assert text.count(old) == 1
    return {'dsplib/v27fax.h': text.replace(old, old.replace('short count);', 'int count);'))}


if __name__ == '__main__':
    driver.REV = REV
    driver.OUT_NAME = 'recovery-v27-data-boundaries'
    driver.SOURCE_PATHS = ('src/fax/V27t_int.c', 'src/fax/V27t_prc.c')
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.DUMP_FLAGS = ()
    driver.main()
