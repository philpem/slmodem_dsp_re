#!/usr/bin/env python3
"""Cross original early run publication with a guarded countdown loop."""
import playbook_small_patterns as d
import unblock_v8_runs as old


def publish(text):
    helpers={
        'drain_run': """drain_run(struct v8_v21_params *p, int *run, short bit)
{
\tunsigned int whole = (unsigned int)*run >> 2;
\tunsigned int need;
\tunsigned int n;

\t*run &= 3;
\tneed = 4 - (whole > 2 ? 3 : whole + 1);
\tn = whole + 1 - ((unsigned int)*run < need);
\tif (n != 0)
\t\tpush_bits(p, n, bit);
}""",
        'flush_run': """flush_run(struct v8_v21_params *p, int *run, short bit)
{
\tunsigned int whole = (unsigned int)*run >> 2;

\t*run &= 3;
\tif (whole != 0)
\t\tpush_bits(p, whole, bit);
}"""}
    for name,fn in helpers.items():
        a,z,_=d.function(text,name)
        assert text[:a].endswith('static int\n')
        text=text[:a-len('static int\n')]+'static void\n'+fn+text[z:]
    for field,bit in [('mark_run','mark_bit'),('space_run','space_bit')]:
        block='v->v21.'+field+' =\n\t\t\t\t\tdrain_run(p, v->v21.'+field+', p->'+bit+');'
        assert text.count(block)==1
        text=text.replace(block,'drain_run(p, &v->v21.'+field+', p->'+bit+');')
        block='v->v21.'+field+' = flush_run(p, v->v21.'+field+', p->'+bit+');'
        assert text.count(block)==1
        text=text.replace(block,'flush_run(p, &v->v21.'+field+', p->'+bit+');')
    return text


def countdown(text):
    a,z,fn=d.function(text,'push_bits')
    assert fn.count('\twhile (n-- != 0) {')==1
    fn=fn.replace('\twhile (n-- != 0) {','\tif (n != 0) {\n\t\tdo {')
    assert fn.endswith('\t}\n}')
    fn=fn[:-len('\t}\n}')]+ '\t\t} while (--n != 0);\n\t}\n}'
    return text[:a]+fn+text[z:]


def variants(path,source):
    cells=old.variants(path,source)
    positive=cells['unsigned-runs-positive-push'];zero=cells['unsigned-runs-zero-only-push']
    result={'baseline':source,'unsigned-positive-control':positive,'unsigned-zero-control':zero,
            'published-zero':publish(zero),'unsigned-countdown':countdown(zero),
            'published-countdown':countdown(publish(zero))}
    assert len(result)==len(set(result.values()))==6
    return result

if __name__=='__main__':
    d.REV='38626248';d.SOURCE_PATHS=('src/v8/V8Dpsk.c',);d.OUT_NAME='v8-remainder-owner'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
