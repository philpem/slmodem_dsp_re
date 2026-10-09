#!/usr/bin/env python3
"""Original consumed sample-index lifetime, without changing FP/store order."""
import playbook_small_patterns as d


def variants(path, source):
    start, end, function = d.function(source, 'V90AutoDigitalImpDetector::addReceivedSampleToStorage')
    old = '\tint n = sampleCount[phase];\n\tshort q = (short)(v + 0.5f);'
    assert function.count(old) == 1
    modified = function.replace(old, '\tint n = sampleCount[phase];\n\tshort *destination = &sampleStore[phase][n];\n\tshort q = (short)(v + 0.5f);')
    assert modified.count('sampleStore[phase][n] =') == 1
    modified = modified.replace('sampleStore[phase][n] =', '*destination =')
    return {'baseline': source, 'early-slot': source[:start]+modified+source[end:],
            'baseline-repeat': source}


if __name__ == '__main__':
    d.REV = 'a95a6c65'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'v90-sample-slot'
    # This TU's previously measured full -da ICE is not a source hypothesis.
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr')
    d.variants = variants
    d.main()
