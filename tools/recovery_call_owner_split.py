#!/usr/bin/env python3
"""Preserve outer buffer producers while reloading the inner block owner."""
import playbook_small_patterns as d

def variants(path, source):
    old = '''call_block(struct call_dp *st)
{
\tshort *from_line = st->in_q.data + st->in_q.active / 2;
\tshort *to_line = st->out_q.data + st->out_q.active / 2;'''
    new = '''call_block(struct dp *dp, short *from_line, short *to_line)
{
\tstruct call_dp *st = ((struct call_dp *)dp)->self;'''
    assert source.count(old) == 1
    candidate = source.replace(old, new)
    old = '\t\t\tint block = call_block(st);'
    assert candidate.count(old) == 1
    candidate = candidate.replace(old, '''\t\t\tint block = call_block(dp,
\t\t\t\tst->in_q.data + st->in_q.active / 2,
\t\t\t\tst->out_q.data + st->out_q.active / 2);''')
    byte_candidate = candidate.replace('st->in_q.data + st->in_q.active / 2', '(short *)((char *)st->in_q.data + st->in_q.active)').replace('st->out_q.data + st->out_q.active / 2', '(short *)((char *)st->out_q.data + st->out_q.active)')
    return {'baseline':source, 'split-owner':candidate, 'split-byte-address':byte_candidate}

if __name__ == '__main__':
    d.REV='f8c8225c'
    d.SOURCE_PATHS=('src/call/call.c',)
    d.OUT_NAME='recovery-call-owner-split'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
