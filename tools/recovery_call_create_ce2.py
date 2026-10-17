#!/usr/bin/env python3
"""One diagnostic second-if-conversion control; never a production flag."""
import playbook_small_patterns as d

if __name__=='__main__':
    d.REV='72307120'
    d.SOURCE_PATHS=('src/call/call.c',)
    d.OUT_NAME='recovery-call-create-ce2'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=lambda path,source:{'baseline':source,'diagnostic-no-ce2':source,'diagnostic-no-if-conversion':source}
    d.CELL_FLAGS=lambda path,label: {'baseline':(), 'diagnostic-no-ce2':('-fno-if-conversion2',), 'diagnostic-no-if-conversion':('-fno-if-conversion',)}[label]
    d.main()
