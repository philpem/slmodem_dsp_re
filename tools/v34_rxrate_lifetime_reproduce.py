#!/usr/bin/env python3
"""Cross original eager session-pointer lifetime with one final return result."""
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'VPcmV34GetCurrentRxBitRate')
    marker = '\tconst struct v34_ratecfg *cfg = &obj->ratecfg;'
    local = '\t\t\tVPcmFloModem *sess = (VPcmFloModem *)obj->p3548;\n\n'
    assert fn.count(marker) == fn.count(local) == 1
    cells = {}
    for eager, common in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        body = fn
        if eager:
            body = body.replace(marker, marker+'\n\tVPcmFloModem *sess = (VPcmFloModem *)obj->p3548;\n\tconst int *k56 = (const int *)obj->pac18;')
            body = body.replace(local, '').replace('*(const int *)obj->pac18', '*k56')
        if common:
            start = body.index('\n\tif (obj->role == PCM_ROLE)')
            prefix = body[:start]+'\n\tint rate;\n'
            pointer = '' if eager else local
            k56 = '*k56' if eager else '*(const int *)obj->pac18'
            body = prefix + ('\n\tif (obj->role == PCM_ROLE) {\n'
                '\t\tif ((unsigned)(obj->status - 1) <= 1) {\n'+pointer+
                '\t\t\trate = (int)sess->modem.demodulator->getBitRate();\n'
                '\t\t} else {\n\t\t\trate = cfg->rxbits * (int)RATE_STEP;\n\t\t}\n'
                '\t} else if (obj->status == 3) {\n\t\trate = '+k56+';\n'
                '\t} else {\n\t\trate = cfg->rxbits * (int)RATE_STEP;\n\t}\n'
                '\treturn rate;\n}')
        cells['baseline' if not (eager or common) else f'eager-{eager}-common-{common}'] = source[:a]+body+source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'b3665999'
    d.OUT_NAME = 'v34-rxrate-lifetime'
    d.SOURCE_PATHS = ('src/pump/v34/VPcmV34Main.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
