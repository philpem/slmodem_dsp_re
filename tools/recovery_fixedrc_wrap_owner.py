#!/usr/bin/env python3
"""Cross aggregate pair recovery with the witnessed circular wrap pointer owner."""
import sys
import playbook_small_patterns as d
from recovery_fixedrc_pair import variants as pair_variants


def variants(path, source):
    pair = pair_variants(path, source)["array"]
    cells = {}
    for aggregate in (False, True):
        for owner in (False, True):
            text = pair if aggregate else source
            if owner:
                old = "cursor = history + count;"
                assert text.count(old) == 1
                text = text.replace(old, "cursor += count;")
            label = "baseline" if not aggregate and not owner else "pair-%d-owner-%d" % (aggregate, owner)
            cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "d9995470"
    d.OUT_NAME = "recovery-fixedrc-wrap-owner"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
