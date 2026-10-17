#!/usr/bin/env python3
"""Replay the original V.92 completed-study diagnostic on a complete period TU."""
import playbook_small_patterns as d

d.REV = 'e0e56d23'
d.OUT_NAME = 'recovery-v92-transition-text'
d.SOURCE_PATHS = ('src/pump/v90/V90Phase3Demodulator.cpp',)
d.DUMP_FLAGS = ()


def variants(path, source):
    begin = source.index('V90Phase3Demodulator::getV92Decision(float sample)')
    end = source.index('\n\tcase 7:', begin)
    old = 'edprintf("V90Phase3Demodulator: JdNot detected " "@ %d\\r\\n", samplesInState);'
    assert source[begin:end].count(old) == 1
    fixed = source[:begin] + source[begin:end].replace(old,
        'edprintf("V90Phase3Demodulator: enter Wait For V92Jd state @ %d\\r\\n", samplesInState);') + source[end:]
    return {'baseline': source, 'original-transition-text': fixed}


d.variants = variants
if __name__ == '__main__':
    d.main()
