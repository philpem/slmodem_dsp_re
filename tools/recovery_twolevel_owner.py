#!/usr/bin/env python3
"""Test original post-isAltRbs owner reloads without changing other methods."""
import playbook_small_patterns as d

d.REV = 'b856fde4'
d.OUT_NAME = 'twolevel-owner'
d.SOURCE_PATHS = ('src/pump/v90/V90Phase3Demodulator.cpp',)
d.DUMP_FLAGS = ('-da', '-fdump-tree-original')


def variants(path, source):
    start = source.index('V90Phase3Demodulator::twoLevelDemod(')
    prefix, body = source[:start], source[start:]
    assert body.count('P3D_LINMAPPALT(ad, ucode)') == body.count('P3D_LINMAPP(ad, ucode)') == 1
    candidate = body.replace('P3D_LINMAPPALT(ad, ucode)', 'P3D_LINMAPPALT(autoDigitalImpDetector, ucode)')
    candidate = candidate.replace('P3D_LINMAPP(ad, ucode)', 'P3D_LINMAPP(autoDigitalImpDetector, ucode)')
    return {'baseline': source, 'postcall-member-owner': prefix + candidate}


d.variants = variants
if __name__ == '__main__':
    d.main()
