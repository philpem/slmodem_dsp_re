#!/usr/bin/env python3
"""Cross CID reset branch placement with its observed zero return boundary."""
import sys,subprocess
import playbook_small_patterns as driver
import playbook_cid_reset_control as branch

def returned(source):
    old='void\ncid_reset(struct cid_modem *ctx)'
    assert source.count(old)==1
    source=source.replace(old,'int\ncid_reset(struct cid_modem *ctx)')
    start=source.index('cid_reset(struct cid_modem *ctx)')
    brace=source.index('{',start);level=1;i=brace+1
    while level:
        if source[i]=='{':level+=1
        elif source[i]=='}':level-=1
        i+=1
    return source[:i-1]+'\treturn 0;\n'+source[i-1:]

def variants(path,source):
    local=branch.variants(path,source)['branch-clear']
    return {'baseline':source,'branch-void':local,'common-int':returned(source),'branch-int':returned(local)}

def overlays(path,label):
    if not label.endswith('-int'):return {}
    rel='dsplib/cid_modem.h';source=(driver.ROOT/'include'/rel).read_text()
    old='void cid_reset(struct cid_modem *ctx);'
    assert source.count(old)==1
    return {rel:source.replace(old,'int cid_reset(struct cid_modem *ctx);')}

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='a7dfe334'
    driver.OUT_NAME='playbook-cid-reset-return'
    driver.SOURCE_PATHS=('src/service/cidcore/cid.c',)
    driver.variants=variants
    driver.HEADER_OVERLAYS=overlays
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
