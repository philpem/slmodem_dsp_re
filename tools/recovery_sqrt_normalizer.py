#!/usr/bin/env python3
"""Cross pointed normalization homes and quotient-based parity in both sqrt bodies."""
import playbook_small_patterns as d

HELPERS = '''static inline void
sqrt_normalize16(unsigned short x, unsigned short *mantissa, unsigned short *count)
{
    *count = 0;
    while (x <= 0x3fff) {
        x += x;
        (*count)++;
    }
    *mantissa = x;
}

static inline void
sqrt_normalize32(unsigned int x, unsigned short *mantissa, unsigned short *count)
{
    *count = 0;
    while (x <= 0x1fffffffU) {
        x += x;
        (*count)++;
    }
    *mantissa = (unsigned short)(x >> 15);
}

'''

def variants(path, source):
    cells = {'baseline': source}
    for pointed in (False, True):
        for quotient in (False, True):
            text = source
            for name,typ,helper in [('FPM_sqrt','unsigned short','16'),('FPM_sqrt_dp','unsigned int','32')]:
                start,end,old=d.function(text,name)
                decl='unsigned short exponent;\n    unsigned short mantissa;\n    unsigned short index;'
                norm='sqrt_normalize'+helper+'(x, &mantissa, &exponent);'
                if not pointed:
                    norm='exponent = 0;\n    while (x <= '+('0x3fff' if helper=='16' else '0x1fffffffU')+') { x += x; exponent++; }\n    mantissa = '+('x' if helper=='16' else '(unsigned short)(x >> 15)')+';'
                parity='if (exponent & 1) mantissa >>= 1;'
                shift='(exponent >> 1)'
                if quotient:
                    decl+='\n    unsigned half;'
                    parity='half = exponent / 2;\n    if (exponent != half * 2) mantissa >>= 1;'
                    shift='half'
                body=name+'('+typ+' x)\n{\n    '+decl+'\n    if (x == 0) return 0;\n    '+norm+'\n    '+parity+'\n    index = ((mantissa + 64) >> 7) - 64;\n'
                if helper=='32':body+='    if (index > 0xbf) index = 0xbf;\n'
                body+='    return (unsigned short)(FPM_sqrt_table[index] >> '+shift+');\n}'
                text=text[:start]+body+text[end:]
            if pointed:text=text.replace('unsigned short\nFPM_sqrt(',HELPERS+'unsigned short\nFPM_sqrt(',1)
            cells[('pointed' if pointed else 'local')+'-'+('quotient' if quotient else 'bit')]=text
    # Original stack homes put count at +0 and mantissa at +2. Cross the
    # two caller declaration orders after the graph-recovery controls.
    cells['pointed-quotient-output-order'] = cells['pointed-quotient'].replace(
        'unsigned short exponent;\n    unsigned short mantissa;',
        'unsigned short mantissa;\n    unsigned short exponent;')
    return cells

if __name__ == '__main__':
    d.REV='95e01c27'
    d.SOURCE_PATHS=('src/dsp/fpm_sqrt.c',)
    d.OUT_NAME='recovery-sqrt-normalizer'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
