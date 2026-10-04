#!/usr/bin/env python3
"""Cross observable MRF formal-slot narrowing with B103 call/owner boundaries."""
import itertools,subprocess
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/dsp/fpm_mrf.c','src/pump/b103/B103prc.c');d.OUT_NAME='batch20-mrf-formal'
axes={}
for formal,returns,owner,cast in itertools.product(('short','unsigned','int'),(False,True),(False,True),(False,True)):
    label='baseline' if formal=='short' and not(returns or owner or cast) else formal+('-returns' if returns else '')+('-owner' if owner else '')+('-count' if cast else '')
    axes[label]=(formal,returns,owner,cast)
def header(rel):return subprocess.check_output(['git','show',d.REV+':include/'+rel],cwd=d.ROOT,text=True)
def overlays(path,label):
    formal,returns,owner,cast=axes[label];out={}
    if formal!='short':
        s=header('dsplib/fpm_mrf.h');assert s.count('short count);')==1
        out['dsplib/fpm_mrf.h']=s.replace('short count);',('unsigned short' if formal=='unsigned' else 'int')+' count);')
    if returns:
        s=header('dsplib/b103fp.h')
        for name in ('ModDataB103','TxNoCarrierB103'):
            assert s.count('short '+name+'(')==1;s=s.replace('short '+name+'(','unsigned short '+name+'(')
        out['dsplib/b103fp.h']=s
    return out
def variants(path,source):
    cells={}
    for label,(formal,returns,owner,cast) in axes.items():
        text=source
        if path.endswith('/fpm_mrf.c'):
            if formal!='short':
                old='FPM_MRF_filter(struct fpm_mrf *state, const short *in, short *out, short count)'
                assert text.count(old)==1;text=text.replace(old,old.replace('short count',('unsigned short' if formal=='unsigned' else 'int')+' count'))
                assert text.count('int remaining = count;')==1;text=text.replace('int remaining = count;','int remaining = (short)count;')
        else:
            for name in ('ModDataB103','TxNoCarrierB103'):
                a,b,fn=d.function(text,name)
                if owner:
                    assert fn.count('\tstruct b103_dsp *dsp = fp->dsp;\n')==1
                    fn=fn.replace('\tstruct b103_dsp *dsp = fp->dsp;\n','').replace('dsp->','fp->dsp->')
                if cast:
                    assert fn.count('(short)nsamples')==1;fn=fn.replace('(short)nsamples','nsamples')
                if returns:
                    assert fn.count('return (short)(unsigned short)FPM_MRF_filter')==1
                    fn=fn.replace('return (short)(unsigned short)FPM_MRF_filter','return FPM_MRF_filter')
                text=text[:a]+fn+text[b:]
                if returns:
                    marker='short\n'+name+'(';assert text.count(marker)==1;text=text.replace(marker,'unsigned short\n'+name+'(')
        cells[label]=text
    assert len(cells)==24
    return cells
d.HEADER_OVERLAYS=overlays;d.variants=variants
if __name__=='__main__':d.main()
