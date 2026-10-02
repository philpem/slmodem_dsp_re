#!/usr/bin/env python3
"""Four complete-TU source/peephole controls; diagnostic flags only."""
import playbook_detsequence_word_home as homes
import json


def variants(path, source):
    cells = homes.variants(path, source)
    return {**cells, 'late-nopeephole': cells['baseline'],
            'early-nopeephole': cells['word-before-mask']}


if __name__ == '__main__':
    driver = homes.lifetime.found.loops.driver
    driver.REV = 'e98ea74b'
    driver.OUT_NAME = 'detsequence-peephole'
    driver.SOURCE_PATHS = ('src/pump/v32/V32prc.c',)
    driver.variants = variants
    original_compile = driver.tc.compile_shell
    def compile_control(path, flags, output, source):
        extra = ['-fno-peephole2'] if 'nopeephole/' in str(source) else []
        return original_compile(path, list(flags) + extra, output, source)
    driver.tc.compile_shell = compile_control
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'V32prc'
    directory.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT / 'build/playbook-detsequence-word-home/V32prc'
    saved = directory / 'retained.o'
    if saved.exists():
        assert saved.read_bytes() == (prior / 'baseline/candidate.o').read_bytes()
    else:
        saved.write_bytes((prior / 'baseline/candidate.o').read_bytes())
    driver.main()
    for label in ('baseline', 'word-before-mask'):
        assert (directory / label / 'candidate.o').read_bytes() == (prior / label / 'candidate.o').read_bytes()
    result = json.loads((directory.parent / 'results.json').read_text())
    assert len(result['families']['V32prc']['cells']) == 4
    for control, alternative in [('baseline', 'late-nopeephole'),
                                  ('word-before-mask', 'early-nopeephole')]:
        objects = [str(directory / name / 'candidate.o') for name in (control, alternative)]
        bodies = [driver.b.body(obj, 'DetSequence') for obj in objects]
        assert bodies[0] != bodies[1], 'known peephole exposure did not fire'
    print('raw full-TU source controls: 2 / 2; peephole exposure controls: 2 / 2')
