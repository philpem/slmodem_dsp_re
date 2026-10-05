#!/usr/bin/env python3
"""Cross independently observed zero-extended input bits with two run controls."""
import playbook_small_patterns as d
import v8_remainder_owner_reproduce as prior


def bits(text):
    assert text.count('short bit)')==3
    return text.replace('short bit)','unsigned short bit)')


def variants(path,source):
    previous=prior.variants(path,source)
    result={'baseline':source}
    for name in ['published-zero','published-countdown']:
        control=previous[name]
        result[name+'-signed-bit']=control
        result[name+'-unsigned-bit']=bits(control)
    assert len(result)==len(set(result.values()))==5
    return result

if __name__=='__main__':
    d.REV='38626248';d.SOURCE_PATHS=('src/v8/V8Dpsk.c',);d.OUT_NAME='v8-bit-extension'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
