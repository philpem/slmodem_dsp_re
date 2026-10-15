#!/usr/bin/env python3
"""Current full-TU transfer of independently witnessed helper boundaries."""
import re
import playbook_small_patterns as d
from fax_next_state_publication_reproduce import variants as next_state
from batch100_fax_gen_eq27 import variants as training

MRF={
 'src/pump/v32/V32int.c':'DemodDataV32',
 'src/pump/b103/B103prc.c':'DemodDataB103',
 'src/fax/V17r_int.c':'DemodDataV17',
 'src/fax/V21r_int.c':'DemodDataV21',
 'src/fax/V27r_int.c':'DemodDataV27',
 'src/fax/V29r_int.c':'DemodDataV29',
 'src/service/Rxcid.c':'cid_modem',
}
RESULT={
 'DemodDataV17':'RXS(modem)->agc.value.signal',
 'DemodDataV27':'((struct v27_rx *)modem)->rx->agc.signal',
 'DemodDataV29':'((struct v29_rx *)modem)->rx->agc.signal',
 'DemodDataV21':'rx->dsp->agc.signal',
}


def variants(path,source):
    if path.endswith('/V21t_prc.c'):
        old=next_state(path,source)
        return {k:old[k] for k in ('baseline','post-diagnostic-owner')}
    if path.endswith('/V27t_prc.c'):
        return training(path,source)
    name=MRF[path];a,z,fn=d.function(source,name);cells={}
    for count in (False,True):
        for result in ((False,True) if name in RESULT else (False,)):
            text=fn
            if count:
                start=text.index('FPM_MRF_filter(');end=text.index(';',start)
                call=text[start:end]
                old='ncount' if name=='cid_modem' else '(short)count'
                assert call.count(old)==1,(path,call)
                text=text[:start]+call.replace(old,'count')+text[end:]
            if result:
                start=text.index('\tFPM_AGC_agc(');end=text.index(';',start)+1
                call=text[start:end].strip()
                lhs='rx->dsp->int_0004' if name=='DemodDataV21' else 'signal'
                assignment='\t'+lhs+' = '+RESULT[name]+';'
                tail=text.index(assignment,end)+len(assignment)
                gap=text[end:text.index(assignment,end)]
                assert not re.sub(r'/\*.*?\*/|\s','',gap,flags=re.S),(path,gap)
                text=text[:start]+'\t'+lhs+' = '+call+text[tail:]
            label='-'.join(k for k,v in [('count',count),('result',result)] if v) or 'baseline'
            cells[label]=source[:a]+text+source[z:]
    if name=='DemodDataV21':
        for label in ('result','count-result'):
            text=cells[label]
            pattern=r'\trx->dsp->int_0004 = (FPM_AGC_agc\([^;]+;)' 
            def cache(match):
                return ('\t{\n\t\tint signal = '+match[1]+'\n\n'
                        '\t\trx->dsp->int_0004 = signal;\n\t}')
            text,n=re.subn(pattern,cache,text)
            assert n==1
            cells[label+'-cache']=text
    return cells


if __name__=='__main__':
    d.REV='ec575202'
    d.SOURCE_PATHS=tuple(MRF)+('src/fax/V21t_prc.c','src/fax/V27t_prc.c')
    d.OUT_NAME='gcc3-boundary-transfer'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
