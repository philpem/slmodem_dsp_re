#!/usr/bin/env python3
"""Cross the supported V34 frame candidates together on a complete TU."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_v34_frame_sequence import variants as controls


def variants(path, source):
    prior=controls(path,source)
    get=prior['get-fixed-sequence-1-buffer-capture']
    put=prior['put-fixed-int']
    a,z,fn=d.function(put,'putFrame');ga,gz,_=d.function(get,'putFrame')
    both=get[:ga]+fn+get[gz:]
    return {'baseline':source,'put-fixed':put,'get-sequence-buffer':get,'both':both}


if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV='f420cd89'
    d.OUT_NAME='v34-frame-retention'
    d.SOURCE_PATHS=('src/pump/v34/v34shell.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
