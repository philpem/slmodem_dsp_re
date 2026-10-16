#!/usr/bin/env python3
"""Separate the original unsigned entry predicate from compiler loop direction."""
import sys
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, "rc_output")
    cells = {}
    for unsigned in (False, True):
        for cursors in (False, True):
            body = fn
            if unsigned:
                assert body.count("\tint k;") == 1
                body = body.replace("\tint k;", "\tunsigned int k;")
            if cursors:
                old = "acc += (int)coeff[k] * (int)hist[k];"
                assert body.count(old) == 1
                body = body.replace(old, "acc += (int)*coeff++ * (int)*hist++;")
            text = source[:start] + body + source[end:]
            label = "baseline" if not unsigned and not cursors else "unsigned-%d-cursors-%d" % (unsigned, cursors)
            cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "a5f306a3"
    d.OUT_NAME = "recovery-fixedrc-index-width"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
