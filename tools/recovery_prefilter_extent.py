#!/usr/bin/env python3
"""Replay original 33-row PreFilter extent and natural successor alignment.

After the header recovery, pass --historical-headers and --baseline-dir pointing
at the saved 2feb98d8 objects/config. The source and header controls stay pinned.
"""
import subprocess
import playbook_small_patterns as d

REV = '2feb98d8'
HEADER = 'include/dsplib/V90PreFilter.h'

def recover(source):
    old = 'V90PreFilter::refLoopsType2[34]'
    assert source.count(old) == 1
    source = source.replace(old, 'V90PreFilter::refLoopsType2[33]')
    start = source.index('\t/*\n\t * NOT IN THE BLOB\'S SYMBOL.')
    end = source.index('\n};', start)
    return source[:start] + source[end:]

def variants(path, source):
    return {'baseline': source, 'original-extent': recover(source)}

def overlays(path, label):
    if label == 'baseline':
        return {}
    header = subprocess.check_output(['git', 'show', REV + ':' + HEADER], cwd=d.ROOT, text=True)
    assert header.count('refLoopsType2[34]') == 1
    return {'dsplib/V90PreFilter.h': header.replace('refLoopsType2[34]', 'refLoopsType2[33]')}

if __name__ == '__main__':
    d.REV = REV
    d.SOURCE_PATHS = ('src/pump/v90/V90PreFilterRefLoops.cpp',)
    d.OUT_NAME = 'recovery-prefilter-extent'
    d.DUMP_FLAGS = ('-v', '-save-temps')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
