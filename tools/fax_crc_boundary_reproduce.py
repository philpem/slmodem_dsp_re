#!/usr/bin/env python3
"""Cross the witnessed CRC pre-OR narrowing with its narrow countdown."""
import playbook_small_patterns as d

def variants(path, source):
    old='(((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12))'
    new='((unsigned short)((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12))'
    assert source.count(old)==1
    cells={'baseline':source}
    for narrow,loop in [(True,False),(False,True),(True,True)]:
        text=source.replace(old,new) if narrow else source
        if loop:
            start,end,fn=d.function(text,'faxvmi_gen_fcs16')
            assert fn.count('for (i = count; i != 0; i--)')==1
            fn=fn.replace('for (i = count; i != 0; i--)','for (i = count; i--; )')
            text=text[:start]+fn+text[end:]
        cells['preor'+str(int(narrow))+'-countdown'+str(int(loop))]=text
    assert len(set(cells.values()))==4
    return cells

if __name__=='__main__':
    d.REV='240481e6';d.SOURCE_PATHS=('src/fax/faxvmi_utls.c',)
    d.OUT_NAME='fax-crc-boundary';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants
    d.main()
