#!/usr/bin/env python3
"""Measure D39 faithful scratch extent without changing the guard."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path, source):
    old = "\tstatic char cEncodedTemp[ENCODE_OUT_MAX + 1];"
    assert source.count(old) == 1
    new = "#ifdef DSPLIB_REPRODUCE_BUGS\n\tstatic char cEncodedTemp[ENCODE_OUT_MAX];\n#else\n" + old + "\n#endif"
    return {"baseline": source, "faithful-extent": source.replace(old, new)}

if __name__ == "__main__":
    assert "--domain" in sys.argv
    d.REV = "4169cb3b"
    d.OUT_NAME = "recovery-encode-extent"
    d.SOURCE_PATHS = ("src/core/encode.c",)
    d.DUMP_FLAGS = ("-v", "-save-temps")
    d.variants = variants
    d.main()
