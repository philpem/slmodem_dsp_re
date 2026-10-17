#!/usr/bin/env python3
"""Recover the original V8 message word zero-extension before shifting."""
import playbook_small_patterns as driver

def variants(path, source):
    old = 'charFlip((unsigned char)(seq->word[i] >> 1))'
    new = 'charFlip((unsigned char)((unsigned short)seq->word[i] >> 1))'
    assert source.count(old) == 1
    return {'baseline': source, 'unsigned-word': source.replace(old, new)}

if __name__ == '__main__':
    driver.REV = '37a77c90'
    driver.OUT_NAME = 'recovery-v8-unsigned-word'
    driver.SOURCE_PATHS = ('src/v8/V8Interface.c',)
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    driver.variants = variants
    driver.main()
