#!/usr/bin/env python3
"""Bounded fax message mask/arm and status value lifetime controls."""
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=tuple('src/fax/Vmi_v'+str(n)+'.c' for n in (17,21,27,29))+('src/fax/V17t_stc.c','src/fax/V29t_stc.c')
d.OUT_NAME='batch50-fax-boundaries'
def variants(path,source):
    cells={'baseline':source}
    if 'Vmi_' in path:
        old='\t\tif ((unsigned)code > (guard))\t\t\t\t\\\n\t\t\t*out = NULL;\t\t\t\t\t\\\n\t\telse\t\t\t\t\t\t\t\\\n\t\t\t*out = (table)[(unsigned char)code];\t\t\\'
        assert old in source
        for validfirst,masked,label in [(False,True,'mask'),(True,False,'valid-first'),(True,True,'valid-first-mask')]:
            value='((unsigned)code & 255)' if masked else '(unsigned char)code'
            if validfirst:
                new='\t\tif ((unsigned)code <= (guard)) \\\n\t\t\t*out = (table)['+value+']; \\\n\t\telse \\\n\t\t\t*out = NULL; \\'
            else:new=old.replace('(unsigned char)code',value)
            cells[label]=source.replace(old,new)
    else:
        for first,final,label in [(True,False,'first-value'),(False,True,'final-value'),(True,True,'both-values')]:
            text=source
            if 'V17' in path:
                marker='\tstatus->flags &= (unsigned char)~V17_STATUS_FLAGS_CLEAR;'
                clear='\tstatus->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;'
                assign='\tstatus->flags = (unsigned char)(p[0x10] & V17_STATUS_FLAG_04);'
                expr='(unsigned char)(p[0x10] & V17_STATUS_FLAG_04)'
                target='status->flags'
                mask='V17_STATUS_FLAGS_CLEAR'
            else:
                target='((struct v29_status_prefix *)status)->flags'
                marker='\t'+target+' &= (unsigned char)~V29STAT_FLAGS_LOW2;'
                clear='\t((struct v29_status_prefix *)status)->flags2 &= (unsigned char)~V29STAT_FLAGS2_BIT0;'
                assign='\t'+target+' =\n\t\t(unsigned char)((*(unsigned char *)(void *)&((struct v29_tx_root *)tx)->cfg.flags) & V29TXS_10_BIT2);'
                expr='(unsigned char)((*(unsigned char *)(void *)&((struct v29_tx_root *)tx)->cfg.flags) & V29TXS_10_BIT2)'
                mask='V29STAT_FLAGS_LOW2'
            assert marker in text and clear in text and assign in text
            body='\t{\n\t\tunsigned char flags;\n'
            body+= '\t\tflags = '+target+';\n\t\tflags &= (unsigned char)~'+mask+';\n\t\t'+target+' = flags;\n' if first else marker+'\n'
            body+= '\t\tflags = '+expr+';\n'+clear+'\n\t\t'+target+' = flags;\n' if final else clear+'\n'+assign+'\n'
            body+='\t}'
            cells[label]=text.replace(marker+'\n'+clear+'\n'+assign,body)
    assert len(set(cells.values()))==len(cells)
    return cells
d.variants=variants
if __name__=='__main__':d.main()
