#!/usr/bin/env python3
"""Cross explicit original quality mask with return ownership, plus multiply control.

F11809's unsigned-result ternaries still branch. The new hypothesis is that
the original SETNE/NEG/AND is an explicit count mask, not just a local width.
Five cells per TU distinguish mask/multiply arithmetic and early/late narrowing.
"""
import playbook_small_patterns as d

def variants(path, source):
    number = next(x for x in ('17', '27', '29') if 'V'+x in path)
    start, end, fn = d.function(source, 'RxHdxDataV'+number)
    variable = 'units' if number == '29' else 'r'
    condition = ('(short)QualityDetectV29(modem) != V29Q_NO_CARRIER'
                 if number == '29' else 'QualityDetectV'+number+'(modem) != V'+number+'_QUALITY_UNRELIABLE')
    assignment = ('\tunits = (short)((short)QualityDetectV29(modem) != V29Q_NO_CARRIER\n'
                  '\t\t\t? (short)n : 0);' if number == '29' else
                  '\tr = (short)('+condition+' ? n : 0);')
    assert fn.count(assignment) == 1
    cells = {'baseline': source}
    for arithmetic in ('mask', 'multiply'):
        for wide in (False, True):
            expression = 'n & (unsigned int)-('+condition+')' if arithmetic == 'mask' else 'n * ('+condition+')'
            new = '\t'+variable+' = '+('('+expression+')' if wide else '(short)('+expression+')')+';'
            changed = fn.replace(assignment, new)
            if wide:
                changed = changed.replace('\tshort '+variable+';', '\tunsigned int '+variable+';')
                changed = changed.replace('\treturn '+variable+';', '\treturn (short)'+variable+';')
            cells[arithmetic+'-wide-%d'%wide] = source[:start]+changed+source[end:]
    assert len(set(cells.values())) == 5
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = tuple('src/fax/V%sr_prc.c'%x for x in ('17', '27', '29'))
    d.OUT_NAME = 'next20-count-mask'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
