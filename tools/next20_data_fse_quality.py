#!/usr/bin/env python3
"""Finite original read-age and per-axis history ownership control."""
import playbook_small_patterns as d

def variants(path, source):
    a,z,fn=d.function(source,'fse_quality')
    owner='\tstruct v32_dec *m = (struct v32_dec *)state->cfg.owner;'
    q='\tint q = state->out_q[(short)state->n_out];'
    history='\tm->sym_q1 = m->sym_q;\n\tm->sym_i1 = m->sym_i;\n\tm->sym_q = (short)q;\n\tm->sym_i = (short)i;'
    assert fn.count(owner)==fn.count(q)==fn.count(history)==1
    cells={'baseline':source}
    for late, paired in ((1,0),(0,1),(1,1)):
        x=fn
        if late:
            x=x.replace(owner,'\tstruct v32_dec *m;').replace(q,q+'\n\tm = (struct v32_dec *)state->cfg.owner;')
        if paired:
            x=x.replace(history,'\tm->sym_q1 = m->sym_q;\n\tm->sym_q = (short)q;\n\tm->sym_i1 = m->sym_i;\n\tm->sym_i = (short)i;')
        label=('late-owner-' if late else '')+('axis-paired-history' if paired else 'history-retained')
        cells[label]=source[:a]+x+source[z:]
    return cells

if __name__=='__main__':
    d.REV='8af3af53';d.SOURCE_PATHS=('src/pump/v32/V32dec.c',);d.OUT_NAME='next20-data-fse-quality'
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
