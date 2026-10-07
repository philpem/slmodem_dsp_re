#!/usr/bin/env python3
"""Separate original multiplied-scale capture from its later publication."""
import playbook_small_patterns as d
from v17_control_value_graph_reproduce import variants as prior


def variants(path, source):
    fixed = prior(path, source)['compound1-owner1-common1']
    start, end, function = d.function(fixed, 'V17TX_control')
    old = '\tblock->pps.cfg.scale *= V17TX_PPS_SCALE[mode];'
    assert function.count(old) == 1
    marker = '\t((struct v17tx_cfg *)fp)->int_0018 = req->int_0010;'
    assert function.count(marker) == 1
    capture = '\tint scaled = block->pps.cfg.scale * V17TX_PPS_SCALE[mode];'
    early = function.replace(old, capture+'\n\tblock->pps.cfg.scale = scaled;')
    late = function.replace(old, capture).replace(marker, marker+'\n\tblock->pps.cfg.scale = scaled;')
    return {'baseline': source, 'compound-owner-common': fixed,
            'capture-early-store': fixed[:start]+early+fixed[end:],
            'capture-original-store': fixed[:start]+late+fixed[end:]}


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/V17t_stc.c',)
    d.OUT_NAME = 'v17-control-scale-publication'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP', '-fsched-verbose=5')
    d.variants = variants
    d.main()
