#!/usr/bin/env python3
"""Two full-TU word-home ordering controls supported by the spill trace."""
import playbook_detsequence_shift_lifetime as lifetime


def variants(path, source):
    source = lifetime.variants(path, source)['separate-decrement']
    start, end, fn = lifetime.found.loops.driver.function(source, 'DetSequence')
    declaration = '\t\tunsigned int word = (unsigned short)*data++;'
    anchor = '\tint mask = hdx->det_mask;'
    assert fn.count(declaration) == fn.count(anchor) == 1
    changed = fn.replace(anchor, '\tunsigned int word;\n' + anchor)
    changed = changed.replace(declaration, '\t\tword = (unsigned short)*data++;')
    return {'baseline': source, 'word-before-mask': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver = lifetime.found.loops.driver
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'playbook-detsequence-word-home'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'V32prc'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-detsequence-shift-lifetime/V32prc/separate-decrement/candidate.o'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
