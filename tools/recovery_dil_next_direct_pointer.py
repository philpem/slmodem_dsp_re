#!/usr/bin/env python3
"""Remove the measured at snapshot from sixteen fixed pointer/publication regions."""
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS
from recovery_dil_next_publication import variants as publication


def variants(path, source):
    v92 = 'V92DIL' in path
    base = publication(path, source)['info-1-owner-1'] if v92 else source
    text = base
    cast = 'unsigned char' if v92 else 'short'
    for index in range(16):
        offset = 8 * index + index // 2
        member = 'segmentSize[%d]' % index if index < 8 else 'segmentCode[%d]' % (index - 8)
        old = '\tdilPackSeven(bits, segmentAt + %d, desc->%s);' % (offset, member)
        new = '''\t{
\t\tint value = desc->%s;
\t\t%s *out = bits + DIL_SEQ1_AT + seq1Bits + seq2Bits + %d;
\t\tint i;

\t\tfor (i = 0; i < 7; i++, value >>= 1)
\t\t\t*out++ = (%s)(value & 1);
\t\tbits[DIL_SEQ1_AT + seq1Bits + seq2Bits + %d] = 0;
\t}''' % (member, cast, offset, cast, offset + 7)
        assert text.count(old) == 1
        text = text.replace(old, new)
    return {'baseline': base, 'direct-pointer-regions': text}


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-direct-pointer'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
