#!/usr/bin/env python3
"""Cross original scalar tap write-back order with the supported output return."""
import subprocess
import sys
import playbook_small_patterns as d
from gcc3_descrambler_output import headers as output_headers, consumers


def variants(path, source):
    return {label:source for label in ('baseline','output','taps','both')}


def headers(path, label):
    if label == 'baseline':
        return {}
    text = (output_headers(path,'both')['dsplib/Scrambler.h'] if label in ('output','both')
            else subprocess.check_output(['git','show','9fabd432:include/dsplib/Scrambler.h'],cwd=d.ROOT,text=True))
    if label in ('taps','both'):
        start = text.index('Descrambler<T, I>::process(T in)')
        end = text.index('\n}',start)
        body = text[start:end]
        old = '\tpTap1--;\n\tpTap2--;'
        assert body.count(old) == 1
        text = text[:start]+body.replace(old,'\tpTap2--;\n\tpTap1--;')+text[end:]
    return {'dsplib/Scrambler.h':text}


if __name__ == '__main__':
    all_consumers = '--all-consumers' in sys.argv
    if all_consumers:
        sys.argv.remove('--all-consumers')
    d.REV = '9fabd432'
    d.SOURCE_PATHS = consumers() if all_consumers else ('src/pump/v90/V90Phase3Demodulator.cpp','src/pump/v90/V90Phase4Demodulator.cpp')
    d.OUT_NAME = 'gcc3-descrambler-scalar-all' if all_consumers else 'gcc3-descrambler-scalar-taps'
    d.DUMP_FLAGS = ('-v',) if all_consumers else ('-v','-save-temps','-da')
    d.variants = (lambda path,source: {label:source for label in ('baseline','both')}) if all_consumers else variants
    d.HEADER_OVERLAYS = headers
    d.main()
