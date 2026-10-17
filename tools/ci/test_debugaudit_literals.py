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
    p.write_text('#define LOG "first\\\nsecond"\nvoid g() { report("after"); }\n')
    spliced = debugaudit.our_strings([str(p)], include_defines=True)
    assert [(line, value) for _, line, value in spliced] == [(1, 'firstsecond'), (3, 'after')]
print('6 literal-scope controls passed: old omission, macro concatenation, ordinary literal, include exclusion, changed text, token splice with line mapping')
