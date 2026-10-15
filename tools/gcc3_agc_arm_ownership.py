#!/usr/bin/env python3
"""Cross AGC shared advance with the witnessed pointer countdowns."""
import playbook_small_patterns as d


def pointer(text):
    assert text.count('\t\tint k;')==1
    text=text.replace('\t\tint k;','\t\tunsigned short k;\n\t\tshort *p;')
    old='\t\t\tfor (k = 0; k < (int)len; k++)\n\t\t\t\tsamples[k] = 0;'
    assert text.count(old)==1
    text=text.replace(old,'\t\t\tp = samples;\n\t\t\tk = len;\n\t\t\twhile (k--)\n\t\t\t\t*p++ = 0;')
    old='\t\tfor (k = 0; k < (int)len; k++) {\n\t\t\tint v = (short)((samples[k] * mult) >> 15);'
    assert text.count(old)==1
    text=text.replace(old,'\t\tp = samples;\n\t\tk = len;\n\t\twhile (k--) {\n\t\t\tint v = (short)((*p * mult) >> 15);')
    for old,new in [('samples[k] = 32767','*p = 32767'),('samples[k] = -32768','*p = -32768'),('samples[k] = (short)v;','*p = (short)v;\n\t\t\tp++;')]:
        assert text.count(old)==1
        text=text.replace(old,new)
    return text


def arm(text):
    old='\t\t\tsamples += len;\n\t\t\tcontinue;\n\t\t}\n\n'
    assert text.count(old)==1
    text=text.replace(old,'\t\t} else {\n')
    start=text.index('\t\tadjusted = (short)(adjusted + 1);')
    end=text.index('\n\t\tsamples += len;',start)
    # Change only lexical nesting; each computation remains in its original order.
    body=text[start:end]
    body='\n'.join('\t'+line if line else line for line in body.split('\n'))
    text=text[:start]+body+'\t\t}\n'+text[end:]
    return text


def variants(path,source):
    cells={'baseline':source,'arm':arm(source),'pointer':pointer(source),
           'both':arm(pointer(source))}
    assert len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    d.REV='45f2a5d6'
    d.SOURCE_PATHS=('src/dsp/fpm_agc.c',)
    d.OUT_NAME='gcc3-agc-arm-ownership'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
