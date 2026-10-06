#!/usr/bin/env python3
"""Replay closed FSE copy forms with non-codegen scheduler diagnostics."""
import playbook_small_patterns as d
import fse_config_copy_reproduce as prior


def variants(path, source):
    cells = prior.variants(path, source)
    return {label: cells[label] for label in ('baseline', 'memcpy-1-delayed-1')}


if __name__ == '__main__':
    d.REV = '6b4509bd'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'fse-scheduler-dependencies'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
