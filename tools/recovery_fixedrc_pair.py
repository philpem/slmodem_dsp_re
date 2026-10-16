#!/usr/bin/env python3
"""Test two witnessed short-aggregate representations in the complete FixedRC TU."""
import re
import sys
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, "RcFixed_Resample")
    cells = {"baseline": source}
    old = "short sample, low, high, value;"
    assert fn.count(old) == 1
    for label, declaration, low, high in (
        ("array", "short sample, value;\n\t\t\t\tshort pair[2];", "pair[0]", "pair[1]"),
        ("struct", "short sample, value;\n\t\t\t\tstruct { short low, high; } pair;", "pair.low", "pair.high"),
    ):
        body = fn.replace(old, "short sample, value;")
        body = re.sub(r"\blow\b", low, body)
        body = re.sub(r"\bhigh\b", high, body)
        assert body.count("short sample, value;") == 1
        body = body.replace("short sample, value;", declaration)
        cells[label] = source[:start] + body + source[end:]
    assert len(cells) == len(set(cells.values())) == 3
    return cells


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "d9995470"
    d.OUT_NAME = "recovery-fixedrc-pair"
    d.SOURCE_PATHS = ("src/core/FixedRC.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.variants = variants
    d.main()
