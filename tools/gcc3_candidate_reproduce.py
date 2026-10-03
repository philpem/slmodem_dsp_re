#!/usr/bin/env python3
"""Replay the three finite V34 candidate families declared in issue248."""
import argparse
import itertools
import sys
import playbook_small_patterns as driver


def variants(path, source):
    global FAMILY
    if FAMILY == 'combined':
        FAMILY='hp'
        hp=variants(path,source)['carry-update-short']
        FAMILY='init'
        combined=variants(path,hp)['sizeof-history']
        FAMILY='combined'
        return {'baseline':source,'combined':combined}
    if FAMILY == 'hp':
        start,end,fn = driver.function(source,'V34TimingHPFilter')
        assert fn.count('\tint k;') == 1
        forms = {'baseline':fn, 'short-index':fn.replace('\tint k;', '\tshort k;')}
        if CARRY_UPDATE:
            old = '\t\tacc = (int)((unsigned)acc\n\t\t\t    + (unsigned)(carry * V34TimingHPFilterCoeff[k]));'
            assert fn.count(old) == 1
            changed = fn.replace(old, '\t\tcarry *= V34TimingHPFilterCoeff[k];\n\t\tacc = (int)((unsigned)acc + (unsigned)carry);')
            forms['carry-update'] = changed
            forms['carry-update-short'] = changed.replace('\tint k;', '\tshort k;')
    elif FAMILY == 'init':
        start,end,fn = driver.function(source,'V34TimingFiltersInit')
        loops = '\tfor (i = 0; i < 6; i++)\n\t\tfor (j = 0; j < 3; j++)'
        begin = fn.index('\tfor (i = 0; i < V34_TIMING_HP_TAPS; i++)')
        stop = fn.index('\n\tt->prefilter_coeff',begin)
        retained = fn[begin:stop]
        direct = '''#ifdef DSPLIB_REPRODUCE_BUGS
	{
		short *state = (short *)((char *)t + 0x24);
		for (i = 0; i < V34_TIMING_INIT_SHORTS; i++)
			state[i] = 0;
	}
#else
	for (i = 0; i < V34_TIMING_HP_TAPS; i++)
		t->hist[i] = 0;
	for (i = 0; i < V34_TIMING_PRE_TAPS; i++)
		t->pre_state[i] = 0;
#endif
'''
        assert fn.count(loops) == 1 and fn.count('\tint i, j;') == 1
        forms = {'baseline':fn}
        for transpose,narrow,wordclear in itertools.product((False,True),repeat=3):
            if not any((transpose,narrow,wordclear)):
                continue
            text=fn
            if transpose:
                text=text.replace(loops,'\tfor (j = 0; j < 3; j++)\n\t\tfor (i = 0; i < 6; i++)')
            if narrow:
                text=text.replace('\tint i, j;','\tshort i, j;')
            if wordclear:
                text=text.replace(retained,direct)
            label='-'.join(name for name,on in (('transpose',transpose),('short',narrow),('wordclear',wordclear)) if on)
            forms[label]=text
        if OPEN_INIT:
            nested=loops+'\n\t\t\tt->iir[i][j] = 0;'
            assert fn.count(nested)==1
            statements='\tfor (i = 0; i < 3; i++) {\n'+''.join('\t\tt->iir[%d][i] = 0;\n'%row for row in range(6))+'\t}'
            opened=fn.replace(nested,statements)
            words=opened.replace('\tint i, j;','\tshort i;\n\tint j;').replace(retained,direct)
            ordered=words.replace('i < V34_TIMING_INIT_SHORTS','i < (unsigned)V34_TIMING_INIT_SHORTS')
            stores='\tt->prefilter_coeff = V34TimingPrefilterCoeff;\n\tt->hp_coeff = V34TimingHPFilterCoeff;'
            assert ordered.count(stores)==1
            ordered=ordered.replace(stores,'\tt->hp_coeff = V34TimingHPFilterCoeff;\n\tt->prefilter_coeff = V34TimingPrefilterCoeff;')
            forms={'baseline':fn,'open-channels':opened,'open-short-words':words,'open-short-words-ordered':ordered}
            if HIST_SIZEOF:
                begin=ordered.index('#ifdef DSPLIB_REPRODUCE_BUGS')
                stop=ordered.index('#else',begin)
                direct_member=ordered[:begin]+'#ifdef DSPLIB_REPRODUCE_BUGS\n\tfor (i = 0; i < sizeof(t->hist); i++)\n\t\tt->hist[i] = 0;\n'+ordered[stop:]
                forms={'baseline':fn,'ordered-control':ordered,'sizeof-history':direct_member}
    else:
        start,end,fn = driver.function(source,'V34GiveProbeResults')
        begin=fn.index('\t\tunion {')
        stop=fn.index('\n\t\tp += V34_PROBE_STRIDE;',begin)
        staged=fn[begin:stop]
        assert fn.count('\tint i, k;')==1 and 'obj->probe_results[i] = u.d;' in staged
        forms={'baseline':fn}
        for direct,narrow in itertools.product((False,True),repeat=2):
            if not (direct or narrow):
                continue
            text=fn
            if direct:
                text=text.replace(staged,'\t\tobj->probe_results[i] = *(const double *)p;')
            if narrow:
                text=text.replace('\tint i, k;','\tshort i;\n\tint k;')
            forms['-'.join(name for name,on in (('direct-double',direct),('short',narrow)) if on)]=text
    result={label:source[:start]+text+source[end:] for label,text in forms.items()}
    assert len(result)==len(set(result.values()))=={'hp':4 if CARRY_UPDATE else 2,'init':3 if HIST_SIZEOF else 4 if OPEN_INIT else 8,'probe':4}[FAMILY]
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(add_help=False)
    parser.add_argument('--family',required=True,choices=('hp','init','probe','combined'))
    parser.add_argument('--carry-update',action='store_true')
    parser.add_argument('--open-init',action='store_true')
    parser.add_argument('--hist-sizeof',action='store_true')
    options,remaining=parser.parse_known_args()
    FAMILY=options.family
    CARRY_UPDATE=options.carry_update or FAMILY=='combined'
    OPEN_INIT=options.open_init or FAMILY=='combined'
    HIST_SIZEOF=options.hist_sizeof or FAMILY=='combined'
    assert not CARRY_UPDATE or FAMILY in ('hp','combined')
    assert not OPEN_INIT or FAMILY in ('init','combined')
    assert not HIST_SIZEOF or OPEN_INIT
    assert '--domain' in remaining
    assert remaining[remaining.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/248')
    sys.argv=sys.argv[:1]+remaining
    driver.REV='4387a6fb'
    driver.OUT_NAME='gcc3-screen-'+FAMILY+('-carry' if CARRY_UPDATE and FAMILY!='combined' else '')+('-open' if OPEN_INIT and FAMILY!='combined' else '')+('-sizeof' if HIST_SIZEOF and FAMILY!='combined' else '')
    driver.SOURCE_PATHS=('src/pump/v34/v34info.c' if FAMILY=='probe' else 'src/pump/v34/v34filters.c',)
    driver.variants=variants
    driver.main()
