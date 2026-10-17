#!/usr/bin/env python3
"""Build unchanged complete V8 interface TU for lifetime-stage diagnosis.

Replay with --baseline-dir pointing to the validated full period object build;
--domain records the bounded unchanged-source hypothesis in results.json.
Run stage_c_v8_lifetime_audit.py afterwards to assert and record the trace.
"""
import playbook_small_patterns as d

d.REV = '4d2efa2d'
d.SOURCE_PATHS = ('src/v8/V8Interface.c',)
d.OUT_NAME = 'stage-c-v8-lifetime'
d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
d.variants = lambda path, source: {'baseline': source}
d.main()
