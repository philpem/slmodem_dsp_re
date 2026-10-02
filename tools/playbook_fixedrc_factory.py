#!/usr/bin/env python3
"""Two full-TU controls for observed FixedRC allocation/dispatch ownership."""
import sys
import playbook_small_patterns as driver
from playbook_fixedrc_reset import variants as reset_variants


def variants(path, source):
    text = reset_variants(path, source)['nonnull-external-clear']
    start, end, fn = driver.function(text, 'RcFixed_Delete')
    assert fn.count('free(') == 5
    text = text[:start] + fn.replace('free(', 'sysdep_free(') + text[end:]
    start, end, fn = driver.function(text, 'RcFixed_Create')
    pairs = [('rc80to96filter',36), ('rc96to80filter',32),
             ('rc80to96filter',36), ('rc480to80filter',68),
             ('rc96to80filter',32), ('rc480to96filter',68),
             ('rc96to384filter',46), ('rc384to96filter',68),
             ('rc80to384filter',40), ('rc384to80filter',68),
             ('rc96to120filter',28), ('rc120to96filter',58),
             ('rc80to120filter',42), ('rc120to80filter',48),
             ('rc96to320filter',38), ('rc320to96filter',68),
             ('rc72to80filter',38), ('rc80to72filter',32)]
    # Forward declarations use the actual same-TU table types and extents.
    declarations = ''.join('static const short '+name+'['+str(size)+'];\n'
                           for name,size in dict(table_extents(source, pairs)).items())
    # The candidate switch is evidence-backed, not a synthesized score table.
    cases = ''
    for mode, (name, taps) in enumerate(pairs, 2):
        cases += ('\tcase '+str(mode)+':\n\t\ts->coeff = '+name+';\n'
                  '\t\ts->taps = '+str(taps)+';\n\t\tbreak;\n')
    body = 'RcFixed_Create(int mode)\n{\n\tstruct rc *h;\n\tstruct rc_state *s = NULL;\n\tstruct rc_kind1 *k1 = NULL;\n\n\tif ((unsigned)mode > 999u)\n\t\treturn NULL;\n\n\th = sysdep_malloc(sizeof(*h));\n\th->state = NULL;\n\tif (mode <= 1) {\n\t\th->kind = 1;\n\t\tk1 = sysdep_malloc(sizeof(*k1));\n\t\th->state = (struct rc_state *)k1;\n\t\tk1->w6 = sysdep_malloc(6);\n\t\tk1->w174 = sysdep_malloc(174);\n\t\tk1->w30 = sysdep_malloc(30);\n\t} else {\n\t\th->kind = 0;\n\t\tif (mode < RCFIXED_NMODES) {\n\t\t\ts = sysdep_malloc(sizeof(*s));\n\t\t\th->state = s;\n\t\t\ts->up = (short)fixedRc_UpFact[mode];\n\t\t\ts->down = (short)fixedRc_DownFact[mode];\n\t\t}\n\t}\n\n\tswitch (mode) {\n\tcase 0:\n\tcase 1:\n\t\tk1->mode = (short)mode;\n\t\tk1->droop = CI_bDroop;\n\t\tk1->b1 = CI_b1;\n\t\tk1->b2 = CI_b2;\n\t\tbreak;\n@CASES@\n\t}\n\tRcFixed_Reset(h);\n\treturn h;\n}'
    body = body.replace('@CASES@', cases.rstrip())
    text = text[:start] + body + text[end:]
    marker = '#include "dsplib/sysdep.h"'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\n\n' + declarations.rstrip())
    return {'baseline': source, 'factory-contract': text}


def table_extents(source, pairs):
    import re
    result = []
    for name, taps in pairs:
        m = re.search(r'static const short '+name+r'\[(\d+)\] =', source)
        assert m, name
        result.append((name, int(m.group(1))))
    return result


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '53c3bd00'
    driver.OUT_NAME = 'playbook-fixedrc-factory'
    driver.SOURCE_PATHS = ('src/core/FixedRC.c',)
    driver.variants = variants
    driver.main()
