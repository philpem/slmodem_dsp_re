#!/usr/bin/env python3
"""Two complete-TU controls for DetSequence's shift/decrement lifetime."""
import playbook_detsequence_found as found


def variants(path, source):
    source = found.variants(path, source)['found-once']
    start, end, fn = found.loops.driver.function(source, 'DetSequence')
    expression = '(word >> shift--)'
    statement = '\t\t\treg = (reg << 1) |\n\t\t\t      (int)((word >> shift--) & 1u);'
    assert fn.count(expression) == fn.count(statement) == 1
    changed = fn.replace(statement, statement.replace(expression, '(word >> shift)')
                         + '\n\t\t\tshift--;')
    return {'baseline': source, 'separate-decrement': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver = found.loops.driver
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'playbook-detsequence-shift-lifetime'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'V32prc'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-detsequence-found/V32prc/found-once/candidate.o'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
