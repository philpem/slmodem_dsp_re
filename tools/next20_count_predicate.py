#!/usr/bin/env python3
"""Give the original quality verdict its own value before count selection.

The first domain's direct mask and multiply fold to conditional control flow
already in01.rtl. A named, used verdict supplies the original SETNE value;
cross mask/multiply with its count use before/after the original flag clear.
No dummy values, changing quality call age, or compiler-option fit.
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
        for after_clear in (False, True):
            changed = fn.replace('\tshort '+variable+';', '\tunsigned int '+variable+';\n\tint reliable;')
            expression = 'n & (unsigned int)-reliable' if arithmetic == 'mask' else 'n * reliable'
            use = '\t'+variable+' = '+expression+';'
            changed = changed.replace(assignment, '\treliable = '+condition+';' + ('' if after_clear else '\n'+use))
            if after_clear:
                clear_end = changed.index(';', changed.index('&= ~V29_STATUS_LOW_SNR' if number == '29' else '&=', changed.index('reliable =')))+1
                changed = changed[:clear_end]+'\n'+use+changed[clear_end:]
            changed = changed.replace('\treturn '+variable+';', '\treturn (short)'+variable+';')
            cells[arithmetic+'-after-clear-%d'%after_clear] = source[:start]+changed+source[end:]
    assert len(set(cells.values())) == 5
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = tuple('src/fax/V%sr_prc.c'%x for x in ('17', '27', '29'))
    d.OUT_NAME = 'next20-count-predicate'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
