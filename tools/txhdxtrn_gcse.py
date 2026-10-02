#!/usr/bin/env python3
"""Cross the input fold with diagnostic GCSE controls on the complete TU."""
import json
import playbook_txhdxtrn as fold


def variants(path, source):
    forms = fold.variants(path, source)
    return {label + suffix if suffix else label: forms[label]
            for label in ('baseline', 'unsigned-word')
            for suffix in ('', '-nolm', '-nogcse')}


if __name__ == '__main__':
    driver = fold.driver
    driver.REV = '3cbe7d52'
    driver.OUT_NAME = 'txhdxtrn-gcse'
    driver.SOURCE_PATHS = ('src/pump/v32/V32TXHDX.c',)
    driver.variants = variants
    original_compile = driver.tc.compile_shell
    def compile_control(path, flags, output, source):
        extra = ['-fno-gcse-lm'] if '-nolm/' in str(source) else (
            ['-fno-gcse'] if '-nogcse/' in str(source) else [])
        return original_compile(path, list(flags) + extra, output, source)
    driver.tc.compile_shell = compile_control
    driver.main()
    directory = driver.ROOT / 'build' / driver.OUT_NAME / 'V32TXHDX'
    prior = driver.ROOT / 'build/playbook-txhdxtrn/V32TXHDX'
    for label in ('baseline', 'unsigned-word'):
        assert (directory / label / 'candidate.o').read_bytes() == (prior / label / 'candidate.o').read_bytes()
    result = json.loads((directory.parent / 'results.json').read_text())
    assert len(result['families']['V32TXHDX']['cells']) == 6
    print('raw full-TU source controls: 2 / 2; compiled cells: 6 / 6')
