#!/usr/bin/env python3
"""Replay the original-supported rate-mask/default publication boundary.

Two cells only: unchanged complete TU and one contiguous block move. Original
mask publication and its diagnostics precede defaults at +0x04c and above.
The retained source explicitly puts those defaults before that block. This
control changes that publication boundary without rearranging scalar stores,
locals, declarations, literals or compiler options. No runtime defect claim.
"""
from pathlib import Path
import sys
import playbook_small_patterns as replay

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'src/pump/v90/V90Parameters.cpp'


def variants(path, source):
    assert path == SOURCE
    start = source.index('\t/*\n\t * The upstream rate window, and the mask it turns into.')
    end = source.index('\n\t/*\n\t * Bit 0 of the modem block', start)
    block = source[start:end + 1]
    before = source[:start] + source[end + 1:]
    marker = '\t/* +0x048 ANALOG_RATE_MASK -- computed, see below */\n'
    assert before.count(marker) == 1
    after = before.replace(marker, marker + block, 1)
    assert source != after, 'block generator did not change the input'
    assert sorted(source.splitlines()) == sorted(after.splitlines())
    return {'baseline': source, 'mask-before-defaults': after}


if __name__ == '__main__':
    replay.REV = '5026710586584479bc3636d2169cfb93445941d4'
    replay.SOURCE_PATHS = (SOURCE,)
    replay.OUT_NAME = 'default-boundary-control'
    replay.variants = variants
    replay.main()
