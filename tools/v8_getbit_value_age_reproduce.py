#!/usr/bin/env python3
"""Cross the original signed-word/captured-count graph with CRC helper reuse.

Declared four cells: unchanged, helper only, word graph only, combined.
Original cmpw/testw, signed word indexing, logical shifts and the CRC-loop
jump bypassing nleft/shifter reloads independently nominate these boundaries.
No register, declaration-order or literal permutations.
"""
import playbook_small_patterns as d

HELPER='\t\t\tc--;\n\t\t\tv8_crc((struct v8_handshake *)s, ((unsigned int)s->shifter >> c) & 1);\n'

def variants(path,source):
    a,b,fn=d.function(source,'v8_getbit')
    result={'baseline':source}
    for graph,helper in [(False,True),(True,False),(True,True)]:
        text=fn
        if graph:
            text=text.replace('\tint remaining;\n\tint loaded = 0;\n\tint pos;',
                '\tshort remaining;\n\tshort loaded = s->nleft;\n\tshort pos;')
            text=text.replace('pos = (unsigned short)s->bitpos;', 'pos = s->bitpos;')
            text=text.replace('remaining = (short)((unsigned short)s->nbits - (unsigned short)pos);','remaining = (short)(s->nbits - pos);')
            text=text.replace('s->nleft = 16;', 's->nleft = loaded = 16;').replace('s->nleft = 4;', 's->nleft = loaded = 4;')
            start=text.index('\tloaded = (unsigned short)s->wordbits;')
            end=text.index('\n\tif (s->crc_enable',start)
            text=text[:start]+'''\tloaded = s->wordbits;
\tif (loaded <= remaining) {
\t\ts->nleft = loaded;
\t\ts->shifter = ((unsigned int)s->shifter << loaded)
\t\t\t     | (unsigned short)s->word[s->wordidx];
\t\ts->wordidx = (short)(s->wordidx + 1);
\t\ts->bitpos = (short)(pos + loaded);
\t} else {
\t\tloaded = remaining;
\t\tif (loaded > 0) {
\t\t\ts->nleft = loaded;
\t\t\ts->shifter = ((unsigned int)s->shifter << loaded)
\t\t\t\t     | (unsigned short)s->word[s->wordidx];
\t\t\ts->bitpos = (short)(pos + loaded);
\t\t}
\t}
'''+text[end:]
            text=text.replace('\t\tint c = loaded;', '\t\tshort c = loaded;')
            text=text.replace('(s->shifter >> c)', '((unsigned int)s->shifter >> c)')
            text=text.replace('s->nleft = (short)(s->nleft - 1);','s->nleft = (short)(loaded - 1);')
            text=text.replace('return (s->shifter >> (short)s->nleft) & 1;', 'return ((unsigned int)s->shifter >> (short)s->nleft) & 1;')
        if helper:
            start=text.index('\t\t\tunsigned crc =')
            end=text.index('\n\t\t} while',start)
            text=text[:start]+HELPER.rstrip('\n')+text[end:]
        label='word%d-helper%d'%(graph,helper)
        result[label]=source[:a]+text+source[b:]
    assert len(result)==len(set(result.values()))==4
    return result

if __name__=='__main__':
    d.REV='fa941457';d.SOURCE_PATHS=('src/v8/V8global.c',)
    d.OUT_NAME='v8-getbit-value-age';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants;d.main()
