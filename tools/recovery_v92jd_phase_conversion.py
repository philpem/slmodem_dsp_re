#!/usr/bin/env python3
"""Replay the V92Jd phase producer/scale cross on the complete Gentoo TU.

Example:
  python3 tools/recovery_v92jd_phase_conversion.py \
      --domain tools/recovery_v92jd_phase_conversion.py \
      --baseline-dir /path/to/5249ea38/tc_repro

The original's unsigned FILDLL followed by FMULS and terminal FSTPS/FLDS
witnesses the conversion boundary.  A double denominator alone does not
exercise it when an earlier float conversion lets GCC fold the graph to SF.
"""
import playbook_small_patterns as driver


def variants(path, source):
    old = '\treturn (float)phase / 65536.0f;'
    assert source.count(old) == 1
    return {
        'baseline': source,
        'float-producer-double-scale': source.replace(
            old, '\treturn (float)phase / 65536.0;'),
        'integer-producer-float-scale': source.replace(
            old, '\treturn phase / 65536.0f;'),
        'integer-producer-double-scale': source.replace(
            old, '\treturn phase / 65536.0;'),
    }


if __name__ == '__main__':
    driver.REV = '5249ea38'
    driver.SOURCE_PATHS = ('src/pump/v90/V92Jd.cpp',)
    driver.OUT_NAME = 'recovery-v92jd-phase-conversion'
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = variants
    driver.main()
