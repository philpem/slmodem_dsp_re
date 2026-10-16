#!/usr/bin/env python3
"""Locate late-scheduler ownership of the remaining null-state argument order."""
import sys
import playbook_small_patterns as d


if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "d9995470"
    d.OUT_NAME = "recovery-null-state-schedule"
    d.SOURCE_PATHS = ("src/fax/cDATAtx.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps", "-da")
    d.CELL_FLAGS = lambda path, label: () if label == "baseline" else ("-fno-schedule-insns2",)
    d.variants = lambda path, source: {"baseline": source, "no-late-schedule": source}
    d.main()
