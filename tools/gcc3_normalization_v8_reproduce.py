#!/usr/bin/env python3
"""Pointed word-count normalization in the V8 AGC, bounded four cells."""
import playbook_small_patterns as d

def variants(path,source):
    a,z,fn=d.function(source,'V8agc')
    marker='''\t\t/* Normalise, remembering by how much. */
\t\twhile (acc <= 0x1fffffff) {
\t\t\tacc += acc;
\t\t\tshift++;
\t\t}
\t\te = (int)(acc >> 15);'''
    assert fn.count(marker)==1
    result={'baseline':source}
    for label,pointed,aux in [('terminal-helper-half-word',False,True),('pointed-helper',True,False),('pointed-helper-half-word',True,True)]:
        helper='''static inline void
normalize_v8_agc(unsigned int value, unsigned short *mantissa, unsigned short *count)
{
\t*count = 0;
\tif (value <= 0x1fffffffu) {
'''
        helper+=('' if pointed else '\t\tint n = 0;\n')
        helper+='''\t\tdo {
\t\t\tvalue += value;
'''
        helper+=('\t\t\t(*count)++;\n' if pointed else '\t\t\tn++;\n')
        helper+='\t\t} while (value <= 0x1fffffffu);\n'
        helper+=('' if pointed else '\t\t*count = (unsigned short)n;\n')
        helper+='\t}\n\t*mantissa = (unsigned short)(value >> 15);\n}\n\n'
        body=fn.replace('\t\tunsigned acc = (unsigned)energy;\n','').replace('\t\tint shift = 0;','\t\tunsigned short shift;').replace('\t\tint e;','\t\tunsigned short e;')
        body=body.replace(marker,'\t\tnormalize_v8_agc((unsigned int)energy, &e, &shift);')
        if aux:
            body=body.replace('\t\tint idx;','\t\tunsigned short idx;\n\t\tunsigned int half_shift;')
            body=body.replace('\t\tif (shift & 1)','\t\thalf_shift = shift >> 1;\n\t\tif (shift != half_shift * 2)')
            body=body.replace('>> (shift >> 1);','>> half_shift;')
        prefix=source[:a];assert prefix.endswith('int\n');prefix=prefix[:-4]+helper+'int\n'
        result[label]=prefix+body+source[z:]
    assert len(set(result.values()))==4
    return result
if __name__=='__main__':
    d.REV='b470429e';d.OUT_NAME='gcc3-normalization-v8';d.SOURCE_PATHS=('src/v8/V8global.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
