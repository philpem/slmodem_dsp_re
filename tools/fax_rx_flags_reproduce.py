#!/usr/bin/env python3
"""Recover the captured upper request byte before the receiver-state store."""
import playbook_small_patterns as d

def variants(path,source):
    text=source
    old='\tcfg->int_0008 = arg->int_0004;\n'
    assert text.count(old)==1
    text=text.replace(old,old+'\n\t{\n\tunsigned char flags = arg->flags;\n')
    text=text.replace('(arg->flags & V21RXCTL_SET_HDX_INT0000) != 0','(flags & V21RXCTL_SET_HDX_INT0000) != 0')
    text=text.replace('if (arg->flags & V21RXCTL_REINIT)','if (flags & V21RXCTL_REINIT)')
    text=text.replace('\treturn 1;\n}', '\t}\n\treturn 1;\n}',1)
    cells={'baseline':source,'captured-byte':text}
    old='\trx->hdx->int_0000 =\n\t\t(arg->flags & V21RXCTL_SET_HDX_INT0000) != 0;'
    new='\t{\n\tint enabled = 0;\n\tif (arg->flags & V21RXCTL_SET_HDX_INT0000)\n\t\tenabled = 1;\n\trx->hdx->int_0000 = enabled;\n\t}'
    assert source.count(old)==1
    cells['guarded-default']=source.replace(old,new)
    cap_old=old.replace('arg->flags','flags')
    assert text.count(cap_old)==1
    cells['captured-guarded']=text.replace(cap_old,new.replace('arg->flags','flags'))
    return cells
if __name__=='__main__':
    d.REV='240481e6';d.SOURCE_PATHS=('src/fax/V21r_stc.c',)
    d.OUT_NAME='fax-rx-flags';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
