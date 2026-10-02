#!/usr/bin/env python3
"""Discriminate the bitreverse statement predicate from an early-folded ternary."""
from pathlib import Path
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'bitreverse')
    old = '\tint out = (v & 1) ? 1 : 0;\n\tshort i;'
    assert fn.count(old) == 1
    statement = fn.replace(old,
        '\tint out = 0;\n\tshort i;\n\n\tif (v & 1)\n\t\tout = 1;')
    return {'baseline': source, 'statement-predicate':
            source[:start] + statement + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'fcf0427a'
    driver.OUT_NAME = 'playbook-v34-bitreverse-init'
    driver.SOURCE_PATHS = ('src/pump/v34/V34TX.c',)
    # Production has the separately validated queue change. Preserve this
    # experiment's declared unchanged full TU at its source revision.
    retained = driver.ROOT / 'build' / driver.OUT_NAME / 'V34TX' / 'retained.o'
    old = driver.ROOT / 'build/playbook-v34-txqueue-adoption/before/src_pump_v34_V34TX.c.o'
    retained.parent.mkdir(parents=True, exist_ok=True)
    if retained.exists():
        assert retained.read_bytes() == old.read_bytes()
    else:
        retained.write_bytes(old.read_bytes())
    driver.variants = variants
    driver.main()
