#!/usr/bin/env python3
"""Cross receive-wrapper truncated SI accumulator and memory count predicate."""
import playbook_small_patterns as d

def variants(path,source):
    mode={'src/fax/V17r_prc.c':'17','src/fax/V21r_prc.c':'21','src/fax/V27r_prc.c':'27'}[path]
    name='V'+mode+'RX_modem';start,end,old=d.function(source,name)
    condition={'17':'left','21':'remaining','27':'n'}[mode]
    cells={'baseline':source}
    for wide,memory in [(True,False),(False,True),(True,True)]:
        fn=old
        if wide:
            assert fn.count('short total')==1
            fn=fn.replace('short total','int total')
            # Existing per-iteration signed-short cast stays intact.
        if memory:
            marker='while ('+condition+' != 0)'
            assert fn.count(marker)==1
            fn=fn.replace(marker,'while (*count != 0)')
        cells['si'+str(int(wide))+'-memory'+str(int(memory))]=source[:start]+fn+source[end:]
    return cells
if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/fax/V17r_prc.c','src/fax/V21r_prc.c','src/fax/V27r_prc.c');d.OUT_NAME='fax-rx-accumulator'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
