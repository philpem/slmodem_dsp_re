#!/usr/bin/env python3
"""Independent short sample owners and original FIR-only fallthrough."""
import playbook_small_patterns as d

def variants(path,source):
    result={'baseline':source}
    if path.endswith('V92PreFilter.cpp'):
        a,z,fn=d.function(source,'V92PreFilter::process')
        old='''\t\tif (tapsIir != 0) {
\t\t\tfir->process(in, tmp, V92PREFILTER_SAMPLES);
\t\t\tiir->process(tmp, out, V92PREFILTER_SAMPLES);
\t\t} else {
\t\t\tfir->process(in, out, V92PREFILTER_SAMPLES);
\t\t}'''
        new='''\t\tif (tapsIir == 0) {
\t\t\tfir->process(in, out, V92PREFILTER_SAMPLES);
\t\t} else {
\t\t\tfir->process(in, tmp, V92PREFILTER_SAMPLES);
\t\t\tiir->process(tmp, out, V92PREFILTER_SAMPLES);
\t\t}'''
        assert fn.count(old)==1;fn=fn.replace(old,new)
        result['fir-only-fallthrough']=source[:a]+fn+source[z:]
    else:
        for old in ['long double sample = *in++;','long double v = *in++;']:assert source.count(old)==1
        result['signed-sample-owner']=source.replace('long double sample = *in++;','short sample = *in++;').replace('long double v = *in++;','short v = *in++;')
    return result
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-input-cfg';d.SOURCE_PATHS=('src/pump/v90/V92PreFilter.cpp','src/pump/v90/V90SpectralShapingFilter.cpp')
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
