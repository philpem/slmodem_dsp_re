#!/usr/bin/env python3
"""Bound flag-clear spelling and original input-byte age in two status leaves."""
import playbook_small_patterns as d


def variants(path, source):
    v17 = path.endswith('V17t_stc.c')
    name = 'V17TX_status' if v17 else 'V29TX_status'
    start, end, function = d.function(source, name)
    target = 'status->flags' if v17 else '((struct v29_status_prefix *)status)->flags'
    mask = 'V17_STATUS_FLAGS_CLEAR' if v17 else 'V29STAT_FLAGS_LOW2'
    oldclear = '\t'+target+' &= (unsigned char)~'+mask+';'
    clear1 = '\tstatus->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;' if v17 else '\t((struct v29_status_prefix *)status)->flags2 &= (unsigned char)~V29STAT_FLAGS2_BIT0;'
    oldvalue = '\tstatus->flags = (unsigned char)(p[0x10] & V17_STATUS_FLAG_04);' if v17 else '\t((struct v29_status_prefix *)status)->flags =\n\t\t(unsigned char)((*(unsigned char *)(void *)&((struct v29_tx_root *)tx)->cfg.flags) & V29TXS_10_BIT2);'
    assert all(function.count(x) == 1 for x in (oldclear, clear1, oldvalue))
    cells = {}
    for form in ('direct', 'local', 'two-clears'):
        for captured in (False, True):
            body = function
            if form == 'local' or captured:
                body = body.replace('{\n', '{\n\tunsigned char flags;\n', 1)
            if form == 'local':
                body = body.replace(oldclear, '\tflags = (unsigned char)('+target+' & (unsigned char)~'+mask+');\n\t'+target+' = flags;')
            if form == 'two-clears':
                one = 'V17_STATUS_FLAG_01' if v17 else '('+mask+' & 0x01)'
                two = 'V17_STATUS_FLAG_02' if v17 else '('+mask+' & 0x02)'
                body = body.replace(oldclear, '\t'+target+' &= (unsigned char)~'+one+';\n\t'+target+' &= (unsigned char)~'+two+';')
            if captured:
                capture = oldvalue.replace(target+' =', 'flags =')
                body = body.replace(clear1+'\n'+oldvalue, capture+'\n'+clear1+'\n\t'+target+' = flags;')
                assert capture in body and oldvalue not in body
            label = 'baseline' if form == 'direct' and not captured else form+'-capture'+str(int(captured))
            cells[label] = source[:start]+body+source[end:]
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/V17t_stc.c', 'src/fax/V29t_stc.c')
    d.OUT_NAME = 'fax-status-flag-age'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
