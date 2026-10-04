#!/usr/bin/env python3
"""Replay sixteen declared state/product-scope timing-prefilter controls."""
import argparse
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as driver


def variants(path,source):
    start,end,fn=driver.function(source,'V34TimingPrefilter')
    re0='\t\tacc_re = (int)((unsigned)acc_re\n\t\t\t       + (unsigned)((short)carry0 * c0));'
    im0='\t\tacc_im = (int)((unsigned)acc_im\n\t\t\t       + (unsigned)((carry0 >> 16) * c0));'
    re1=re0.replace('carry0','carry1').replace('c0','c1')
    im1=im0.replace('carry0','carry1').replace('c0','c1')
    state='''\t\tint old0 = t->pre_state[k];
\t\tint old1 = t->pre_state[k + 1];
\t\tint c0 = V34TimingPrefilterCoeff[k];
\t\tint c1 = V34TimingPrefilterCoeff[k + 1];

\t\tt->pre_state[k] = carry0;
\t\tt->pre_state[k + 1] = carry1;'''
    exchange='''\t\tint old0, old1, c0, c1;

\t\told0 = t->pre_state[k];
\t\tt->pre_state[k] = carry0;
\t\told1 = t->pre_state[k + 1];
\t\tt->pre_state[k + 1] = carry1;
\t\tc0 = V34TimingPrefilterCoeff[k];
\t\tc1 = V34TimingPrefilterCoeff[k + 1];'''
    assert all(fn.count(v)==1 for v in (re0,im0,re1,im1,state))
    forms={}
    for local,interleave,inplace,late in itertools.product((False,True),repeat=4):
        text=fn
        if interleave:text=text.replace(state,exchange)
        if late:
            text=text.replace('\t\tint c0 = V34TimingPrefilterCoeff[k];','\t\tint c0;').replace('\t\tint c1 = V34TimingPrefilterCoeff[k + 1];','\t\tint c1;')
            text=text.replace('\t\tc0 = V34TimingPrefilterCoeff[k];\n','').replace('\t\tc1 = V34TimingPrefilterCoeff[k + 1];\n','')
            text=text.replace(re0,'\t\tc0 = V34TimingPrefilterCoeff[k];\n'+re0).replace(re1,'\t\tc1 = V34TimingPrefilterCoeff[k + 1];\n'+re1)
        if inplace:
            for n,old in [(0,im0),(1,im1)]:
                text=text.replace(old,'\t\tcarry%d >>= 16;\n\t\tcarry%d *= c%d;\n\t\tacc_im = (int)((unsigned)acc_im + (unsigned)carry%d);'%(n,n,n,n))
        if local:
            text=text.replace('\tint k;','\tint k;\n\tint *state = t->pre_state;').replace('t->pre_state[k','state[k')
        label='-'.join(n for n,v in [('state-base',local),('exchange',interleave),('in-place',inplace),('late-coeff',late)] if v) or 'baseline'
        forms[label]=source[:start]+text+source[end:]
    assert len(forms)==len(set(forms.values()))==16
    if INITIALIZATION:
        selected=forms['state-base-exchange-late-coeff']
        first='\tint acc_re = 0x2000;\t\t/* Q14 round-to-nearest, both parts */\n\tint acc_im = 0x2000;'
        assert selected.count(first)==1
        forms={'baseline':source, 'state-base':forms['state-base'],
               'state-base-exchange-late-coeff':selected,
               'imag-first':selected.replace(first,'\tint acc_im = 0x2000;\t\t/* Q14 round-to-nearest, both parts */\n\tint acc_re = 0x2000;'),
               'shared-init':selected.replace(first,'\tint acc_re, acc_im;\n\tacc_re = acc_im = 0x2000;')}
        assert len(forms)==len(set(forms.values()))==5
    return forms


if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False)
    parser.add_argument('--initialization',action='store_true')
    opts,remaining=parser.parse_known_args()
    INITIALIZATION=opts.initialization
    sys.argv=sys.argv[:1]+remaining
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='5fac8659'
    driver.OUT_NAME='gcc3-v34-prefilter-boundaries'+('-initialization' if INITIALIZATION else '')
    driver.SOURCE_PATHS=('src/pump/v34/v34filters.c',)
    driver.variants=variants
    driver.main()
