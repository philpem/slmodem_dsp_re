#!/usr/bin/env python3
"""V92CP/V90Jd ctor definition-position family (TU-position carrier probe).

Posted as the "V92CP/V90Jd ctor TU-position family" comment in #22. All cells
move one constructor definition; behavior is identical. No fuzzing or mutation
execution.
"""
import playbook_small_patterns as driver

REV = '438af9b7'
OUT_NAME = 'playbook-ctor-tu-position'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V92CP.cpp', 'src/pump/v90/V90Jd.cpp')

V92CP_CTOR = 'V92CP::V92CP()\n{'
V90JD_CTOR = 'V90Jd::V90Jd(V90Parameters *params)\n{'


def extract_fn(source, header):
    """Span of the definition starting at `header`, through its closing brace."""
    start = source.index(header)
    i = source.index('{', start)
    depth, j = 0, i
    while True:
        c = source[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    return start, source.index('\n', j) + 1


def move_after(source, header, anchor):
    s, e = extract_fn(source, header)
    fn = source[s:e]
    rest = source[:s] + source[e:]
    _, ae = extract_fn(rest, anchor)
    return rest[:ae] + fn + rest[ae:]


def move_end(source, header):
    s, e = extract_fn(source, header)
    fn = source[s:e]
    rest = source[:s] + source[e:]
    if not rest.endswith('\n'):
        rest += '\n'
    return rest + fn


def variants(path, source):
    if path.endswith('V92CP.cpp'):
        cells = {'baseline': source}
        for label, anchor in [('after-resetDetector', 'V92CP::resetDetector()\n{'),
                              ('after-reset', 'V92CP::reset()\n{'),
                              ('after-resetCRC', 'V92CP::resetCRC()\n{')]:
            cells[label] = move_after(source, V92CP_CTOR, anchor)
        cells['end'] = move_end(source, V92CP_CTOR)
    else:
        cells = {'baseline': source,
                 'last': move_end(source, V90JD_CTOR)}
    assert len(cells) == len(set(cells.values()))
    return cells


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants

if __name__ == '__main__':
    driver.main()
