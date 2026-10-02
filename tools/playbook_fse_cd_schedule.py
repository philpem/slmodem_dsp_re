#!/usr/bin/env python3
"""Four full-TU source/sched2 controls; alternative flag is diagnostic only."""
import playbook_small_patterns as driver
from playbook_fse_cd_conversion import variants as conversion_variants


def variants(path, source):
    recovered = conversion_variants(path, source)['short-count-carrier-counter-use']
    return {'baseline': source, 'recovered': recovered,
            'baseline-nosched2': source, 'recovered-nosched2': recovered}


if __name__ == '__main__':
    driver.REV = 'b1632e79'
    driver.OUT_NAME = 'playbook-fse-cd-schedule'
    driver.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    driver.variants = variants
    compile_shell = driver.tc.compile_shell

    def compile_control(compiler, flags, output, source):
        if '-nosched2/' in output:
            flags = flags + ['-fno-schedule-insns2']
        return compile_shell(compiler, flags, output, source)

    driver.tc.compile_shell = compile_control
    driver.main()
