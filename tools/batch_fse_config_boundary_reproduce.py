#!/usr/bin/env python3
"""Bound FSE configuration publication after original four mode stores."""
import playbook_small_patterns as d

def variants(path, source):
    a,z,fn=d.function(source,'FPM_FSE_init')
    copy='\tstate->cfg = *cfg;\n\n'
    boundary='\tstate->lms_on = 1;\n'
    assert fn.count(copy)==fn.count(boundary)==1
    moved=fn.replace(copy,'').replace(boundary,boundary+'\n'+copy)
    return {'baseline':source,'copy-after-mode-stores':source[:a]+moved+source[z:]}

if __name__=='__main__':
    d.REV='240481e6';d.SOURCE_PATHS=('src/dsp/fpm_fse.c',)
    d.OUT_NAME='batch-fse-config-boundary';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants;d.main()
