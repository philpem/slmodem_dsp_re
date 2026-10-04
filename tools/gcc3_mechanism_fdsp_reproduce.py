#!/usr/bin/env python3
"""Pinned full-TU wrapper/control inputs for observational GCC scratch tracing."""
import gcc3_batch100_fdsp_conversion_helpers as prior
from pathlib import Path
prior.d.REV='9f1199b5'
prior.d.OUT_NAME='gcc3-mechanism-fdsp'
prior.d.DUMP_FLAGS=('-v','-save-temps','-da')
original=prior.variants
def variants(path,source):
 cells=original(path,source)
 return {key:cells[key] for key in ('baseline','in-1-out-1')}
prior.d.variants=variants
if __name__=='__main__':prior.d.main()
