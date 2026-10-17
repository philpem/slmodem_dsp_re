#!/usr/bin/env python3
"""Test the original timer narrowing against a real elapsed-clock failure.

The original stores/reloads the single-precision sum before its duration
comparison. The current SF expression retains excess precision. Compare the
current expression, an explicit float cast alone, and unsuffixed 0.125 (a DF
sum converted to the existing float local). No return-type/profile changes.
Prior timer spelling controls had no exact gain; a constructor-created period
failure is the new discriminator, not a size score.
"""
import playbook_small_patterns as d

d.REV = '50267105'
d.OUT_NAME = 'recovery-tone-timer'
d.SOURCE_PATHS = ('src/service/TONE.c',)
d.DUMP_FLAGS = ('-da', '-save-temps', '-v')


def variants(path, source):
    old = 'tm = (float)n * 0.125f + t->elapsed;'
    assert source.count(old) == 1
    return {
        'baseline': source,
        'explicit-float-cast': source.replace(old,
            'tm = (float)((float)n * 0.125f + t->elapsed);'),
        'double-literal': source.replace(old, 'tm = n * 0.125 + t->elapsed;'),
    }


d.variants = variants
if __name__ == '__main__':
    d.main()
