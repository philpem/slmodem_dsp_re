#!/usr/bin/env python3
"""Cross the evidenced notch addition tree with the postreload scheduler."""
import sys
import playbook_small_patterns as driver
from playbook_notch_grouping import variants as grouping


def variants(path, source):
    recovered = grouping(path, source)['feedback-first']
    return {'baseline': source, 'feedback-first': recovered,
            'baseline-nosched2': source, 'feedback-first-nosched2': recovered}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'b09cadf7'
    driver.OUT_NAME = 'playbook-notch-schedule'
    driver.SOURCE_PATHS = ('src/dsp/Notch.c',)
    driver.variants = variants
    compile_shell = driver.tc.compile_shell

    def compile_control(compiler, flags, output, source):
        if '-nosched2/' in output:
            flags = flags + ['-fno-schedule-insns2']
        return compile_shell(compiler, flags, output, source)

    driver.tc.compile_shell = compile_control
    driver.main()
