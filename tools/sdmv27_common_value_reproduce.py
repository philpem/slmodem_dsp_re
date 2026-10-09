#!/usr/bin/env python3
"""A single config-result assignment, preserving the original later null read."""
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'SDMv27_init')
    old = '\tif (cfg != 0)\n\t\tsdm->nbits = cfg->nbits;\n\telse\n\t\tsdm->nbits = SDMv27_CFG.nbits;'
    assert fn.count(old) == 1
    new = '\tsdm->nbits = cfg != 0 ? cfg->nbits : SDMv27_CFG.nbits;'
    return {'baseline': source, 'common-value': source[:start]+fn.replace(old, new)+source[end:]}

if __name__ == '__main__':
    d.REV = '90c7b22e'
    d.OUT_NAME = 'sdmv27-common-value'
    d.SOURCE_PATHS = ('src/fax/V27_SDM.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
