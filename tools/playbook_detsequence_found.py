#!/usr/bin/env python3
"""Two found-flag lifetime controls on the complete DetSequence TU."""
import playbook_detsequence as loops


def variants(path, source):
    source = loops.variants(path, source)['both']
    start, end, fn = loops.driver.function(source, 'DetSequence')
    assert fn.count('\t\tint found = 0;') == fn.count('\tshort nread = 0;') == 1
    changed = fn.replace('\t\tint found = 0;\n', '')
    changed = changed.replace('\tshort nread = 0;', '\tint found = 0;\n\tshort nread = 0;')
    return {'baseline': source, 'found-once': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver = loops.driver
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'playbook-detsequence-found'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'V32prc'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-detsequence/V32prc/both/candidate.o'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
    from detsequence_analysis import analyze
    analyze(driver.ROOT, driver.OUT_NAME)
