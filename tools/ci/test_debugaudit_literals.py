#!/usr/bin/env python3
"""Controls for lexical diagnostic presence, including header macros."""
from pathlib import Path
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import debugaudit

with tempfile.TemporaryDirectory() as folder:
    p = Path(folder)/'diagnostics.h'
    p.write_text('#include "not-a-message.h"\n#define NOTICE "first " \\\n "second\\r\\n"\nvoid f() { report("ordinary"); }\n')
    ordinary = {x[2] for x in debugaudit.our_strings([str(p)])}
    expanded = {x[2] for x in debugaudit.our_strings([str(p)], include_defines=True)}
    assert 'first second\r\n' not in ordinary
    assert 'first second\r\n' in expanded
    assert 'ordinary' in expanded
    assert 'not-a-message.h' not in expanded
    p.write_text(p.read_text().replace('second', 'WRONG'))
    wrong = {x[2] for x in debugaudit.our_strings([str(p)], include_defines=True)}
    assert 'first second\r\n' not in wrong
print('5 literal-scope controls passed: old omission, macro concatenation, ordinary literal, include exclusion, changed text')
