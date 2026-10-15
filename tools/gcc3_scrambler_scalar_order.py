#!/usr/bin/env python3
"""Check uniform scalar decrement-order transfer with an exact HH control."""
import subprocess
import sys
import playbook_small_patterns as d
from gcc3_descrambler_output import consumers
from gcc3_descrambler_scalar_taps import headers as descrambler_headers


def variants(path,source):
    return {label:source for label in ('baseline','tap2-first')}


def apply_order(text):
    start=text.index('I Scrambler<T, I>::process(T in)')
    end=text.index('\n}',start)
    body=text[start:end]
    old='\tpTap1--;\n\tpTap2--;'
    assert body.count(old)==1
    return text[:start]+body.replace(old,'\tpTap2--;\n\tpTap1--;')+text[end:]


def headers(path,label):
    if label=='baseline':
        return {}
    text=subprocess.check_output(['git','show','9fabd432:include/dsplib/Scrambler.h'],cwd=d.ROOT,text=True)
    return {'dsplib/Scrambler.h':apply_order(text)}


if __name__=='__main__':
    all_consumers = '--all-consumers' in sys.argv
    if all_consumers:
        sys.argv.remove('--all-consumers')
    d.REV='9fabd432'
    d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp','src/pump/v90/V92Phase3Modulator.cpp',
                    'src/pump/v90/V92Phase4Modulator.cpp')
    if all_consumers:
        d.SOURCE_PATHS=consumers()
    d.OUT_NAME='gcc3-scrambler-scalar-all' if all_consumers else 'gcc3-scrambler-scalar-order'
    d.DUMP_FLAGS=('-v',) if all_consumers else ('-v','-save-temps','-da')
    d.variants=variants
    d.HEADER_OVERLAYS=(lambda path,label: {} if label=='baseline' else
                      {'dsplib/Scrambler.h':apply_order(descrambler_headers(path,'both')['dsplib/Scrambler.h'])}) if all_consumers else headers
    d.main()
