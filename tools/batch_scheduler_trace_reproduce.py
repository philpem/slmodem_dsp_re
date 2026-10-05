#!/usr/bin/env python3
"""Replay V8 coefficient capture with diagnostic-only scheduler verbosity."""
import playbook_small_patterns as d
import v8_product_capture_reproduce as prior

def variants(path, source):
    old = prior.variants(path, source)
    return {label: old[label] for label in ('baseline', 'coefficient-first-expressions')}

if __name__ == '__main__':
    d.REV='240481e6'
    d.SOURCE_PATHS=('src/v8/V8Dpsk.c',)
    d.OUT_NAME='batch-scheduler-trace'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP','-fsched-verbose=5')
    d.variants=variants
    d.main()
