#!/usr/bin/env python3
"""Recover V21 next-state owners published by diagnostic callbacks."""
import re
import playbook_small_patterns as d

def variants(path,source):
    side='rx' if 'V21r_' in path else 'tx';name=side.title()+'NextStateV21'
    start,end,fn=d.function(source,name)
    pattern=r'\t\tif \(DSPLIB_DEBUG_ON\(\)\)\n\t\t\tdsplibs_debug_printf\("V21'+side.upper()+r'_STATE_[A-Z]+\\n"\);'
    def repl(match):
        old=match.group();return old.replace('DSPLIB_DEBUG_ON())','DSPLIB_DEBUG_ON()) {')+'\n\t\t\thdx = '+side+'->hdx;\n\t\t}'
    fn,n=re.subn(pattern,repl,fn);assert n==3,n
    owner=source[:start]+fn+source[end:]
    cells={'baseline':source,'post-diagnostic-owner':owner}
    for label,text in [('int-dispatch-fresh-default',source),('owner-int-dispatch',owner)]:
        a,b,part=d.function(text,name)
        if side=='rx':
            marker='\tstruct v21_rx_hdx *hdx = rx->hdx;'
            assert part.count(marker)==1
            part=part.replace(marker,marker+'\n\tint state = hdx->state;')
            part=part.replace('switch (hdx->state)','switch (state)')
        else:
            assert part.count('short state = hdx->state;')==1
            part=part.replace('short state = hdx->state;','int state = hdx->state;')
            part=part.replace('"V21TX_DEFAULT, %d\\n", state);','"V21TX_DEFAULT, %d\\n", hdx->state);')
        cells[label]=text[:a]+part+text[b:]
    assert len(cells)==len(set(cells.values()))==4
    return cells
if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/fax/V21r_prc.c','src/fax/V21t_prc.c');d.OUT_NAME='fax-next-state-publication'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
