#!/usr/bin/env python3
"""Replay eight object-derived quotient/comparison/accumulation boundaries."""
import argparse,itertools,sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={}
    for member,pred,outfirst in itertools.product((False,True),repeat=3):
        text=source
        if member:
            for i in (1,2):
                old='\tn = samples%d / blockLen_;\n\tif (n * blockLen_ < samples%d)\n\t\tn++;\n\tblocks%d = n;'%(i,i,i)
                new='\tblocks%d = samples%d / blockLen_;\n\tif (blocks%d * blockLen_ < samples%d)\n\t\tblocks%d++;'%(i,i,i,i,i)
                assert text.count(old)==1
                text=text.replace(old,new)
            text=text.replace('\tunsigned int n;\n','')
        start=text.index('int GenericToneDetector::process(float sample)')
        end=text.index('\n}\n',start)+2
        fn=text[start:end]
        if outfirst:
            a='\tfloat in = acc_0c + sample * sample;\n\tfloat out = acc_10 + y * y;'
            assert fn.count(a)==1
            fn=fn.replace(a,'\tfloat out = acc_10 + y * y;\n\tfloat in = acc_0c + sample * sample;')
        if pred:
            marker='\t\tacc_14 = 0.7f * acc_14 + 0.3f * meanIn;'
            fn=fn.replace(marker,'\t\tbool strong = meanOut >= threshold;\n\n'+marker)
            fn=fn.replace('if (meanOut >= threshold)','if (strong)')
        text=text[:start]+fn+text[end:]
        label='-'.join(n for n,v in [('member-quotient',member),('predicate-first',pred),('output-first',outfirst)] if v) or 'baseline'
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==8
    if RESET:
        member_source=cells['member-quotient']
        cells={}
        for member,late,shared in itertools.product((False,True),repeat=3):
            text=member_source if member else source
            block='\tcount_2c = 0;\n\tacc_0c = 0;\n\tacc_10 = 0;\n\tacc_14 = 0;\n\tacc_18 = 0;'
            accum='\tacc_0c = acc_10 = acc_14 = acc_18 = 0;' if shared else block.split('\n',1)[1]
            replacement=accum+'\n\tcount_2c = 0;' if late else '\tcount_2c = 0;\n'+accum
            assert text.count(block)==1
            text=text.replace(block,replacement)
            label='-'.join(n for n,v in [('member-quotient',member),('late-count',late),('shared-zero',shared)] if v) or 'baseline'
            cells[label]=text
        assert len(cells)==len(set(cells.values()))==8
    return cells

if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False);parser.add_argument('--reset-cross',action='store_true')
    args,rest=parser.parse_known_args(); RESET=args.reset_cross;sys.argv=sys.argv[:1]+rest
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='80c5dea3'; driver.OUT_NAME='gcc3-tone-detector-boundaries'+('-reset' if RESET else '')
    driver.SOURCE_PATHS=('src/dsp/GenericToneDetector.cpp',)
    driver.variants=variants; driver.main()
