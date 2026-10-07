#!/usr/bin/env python3
"""Cross original quality count publication, member tests and AGC narrowing."""
import itertools,re
import playbook_small_patterns as d
NAMES={'17':('qcount','qavg','r'), '27':('q_count','q_acc','verdict'), '29':('dec_error_n','dec_error_avg','verdict')}
def fixed_owner(n,fn):
    if n=='29':return fn
    if n=='17':
        old='''if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V17 Dec error too big..." " unreliable data\\n");'''
        new=old.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {')+'\n\t\t\trx = RXSTATE(modem);\n\t\t}'
    else:
        old='''if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Dec error too big..." " unreliable data\\n");'''
        new=old.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {')+'\n\t\t\trx = ((struct v27_rx *)modem)->rx;\n\t\t}'
    assert fn.count(old)==1;return fn.replace(old,new)
def split_tails(n,fn):
    count,avg,result=NAMES[n]
    avg_start=fn.index('\t\trx->'+avg+' = (short)')
    avg_end=fn.index(';',avg_start)+1
    average=fn[avg_start:avg_end]
    if n=='17':
        seed='err';limit='V17RXS_QCOUNT_SETTLE';judge='V17RXS_QCOUNT_JUDGE';condition='rx->qavg <= (short)(unsigned short)rx->quality_threshold';flag='r4fb2';countcast='short'
    elif n=='27':
        seed='mse';limit='0x31';judge='0x32';condition='rx->q_acc <= (short)rx->q_limit';flag='q_flag';countcast='unsigned short'
    else:
        seed='(short)err';limit='V29Q_AVG_BLOCKS';judge='V29Q_VERDICT_BLOCK';condition='rx->dec_error_avg <= rx->dec_error_limit';flag='short_4f62';countcast='short'
    begin=fn.index('\tif (n == 0)')
    replacement=f'''\tif (n == 0) {{
\t\trx->{avg} = {seed};
\t\trx->{count} = 1;
\t\treturn {result};
\t}}
\tif ((short)n <= {limit}) {{
{average}
\t\trx->{count} = ({countcast})(n + 1);
\t\treturn {result};
\t}}
\tif ((short)n == {judge}) {{
\t\tif ({condition})
\t\t\trx->{flag} = 1;
\t\trx->{count} = ({countcast})(rx->{count} + 1);
\t}}
\treturn {result};
}}'''
    return fn[:begin]+replacement

def variants(path,source):
    n=path.split('/')[-1][1:3];start,end,fn=d.function(source,'QualityDetectV'+n)
    fixed=fixed_owner(n,fn);cells={'baseline':source}
    for tail,member,narrow in itertools.product((False,True),repeat=3):
        if n=='17' and narrow:continue
        x=split_tails(n,fixed) if tail else fixed
        if member:
            field=NAMES[n][0]
            x=re.sub(r'\bif \((\(short\))?n (==|<=|>|!=)',lambda m:'if ((short)rx->'+field+' '+m[2],x)
            x=x.replace('else if ((short)n','else if ((short)rx->'+field)
        if narrow:
            if n=='27':
                old='(short)((&rx->agc)->signal & (&rx->sre)->active)'
                new='(short)((short)(&rx->agc)->signal & (&rx->sre)->active)'
            else:
                old='(short)(rx->sre.active & rx->agc.signal)'
                new='(short)(rx->sre.active & (short)rx->agc.signal)'
            assert x.count(old)==1;x=x.replace(old,new)
        label=f'tail-{int(tail)}-member-{int(member)}-narrow-{int(narrow)}'
        changed=source[:start]+x+source[end:]
        if changed==source:continue
        cells[label]=changed
    assert len(cells)==len(set(cells.values())),(path,'duplicate source cells')
    return cells
if __name__=='__main__':
    d.REV='a95a6c65';d.SOURCE_PATHS=('src/fax/V17r_int.c','src/fax/V27r_int.c','src/fax/V29r_int.c');d.OUT_NAME='fax-quality-use'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
