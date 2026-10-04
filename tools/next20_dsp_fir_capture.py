#!/usr/bin/env python3
"""Retained scalar FIR coefficient/count ownership before input publication."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'FloatFIR::process')
    # First overload is block, so isolate exact scalar signature instead.
    start=source.index('\nFloatFIR::process(float in)')+1
    end=source.index('\n}\n',start)+2
    fn=source[start:end]
    old='\tfloat out;\n\n\th[i] = in;'
    assert fn.count(old)==1
    new='\tfloat out;\n\tconst float *c = coefficients;\n\tconst unsigned int n = taps;\n\n\th[i] = in;'
    fn=fn.replace(old,new).replace('floatfir_convolve(h + i, coefficients, taps)','floatfir_convolve(h + i, c, n)').replace('bufferLength - taps','bufferLength - n').replace('floatfir_carry_tail(h, taps, bufferLength)','floatfir_carry_tail(h, n, bufferLength)')
    return {'baseline':source,'captured-coefficients-count':source[:start]+fn+source[end:]}

if __name__=='__main__':
    d.REV='8af3af53'
    d.SOURCE_PATHS=('src/dsp/FloatFIR.cpp',)
    d.OUT_NAME='next20-dsp-fir-capture'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
