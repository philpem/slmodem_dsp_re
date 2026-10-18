#!/usr/bin/env python3
"""Compile unchanged retained Sgd.c with saved actual driver commands for tracing."""
import playbook_small_patterns as d

d.REV='a988129c0'
d.OUT_NAME='sgd-scratch293-input'
d.SOURCE_PATHS=('src/fax/Sgd.c',)
d.DUMP_FLAGS=('-v','-save-temps','-da')
d.variants=lambda path,source:{'baseline':source}
d.main()
