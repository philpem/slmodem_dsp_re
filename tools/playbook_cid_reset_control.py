#!/usr/bin/env python3
"""Test branch-local fill clearing at the observed CID reset boundary."""
import sys,subprocess
import playbook_small_patterns as driver

def variants(path,source):
    old='\tctx->samples_fill = 0;\n\tif (ctx->mode == CID_MODE_AUTOMATIC)\n\t\tctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;'
    assert source.count(old)==1
    new='\tif (ctx->mode == CID_MODE_AUTOMATIC) {\n\t\tctx->samples_fill = 0;\n\t\tctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;\n\t} else {\n\t\tctx->samples_fill = 0;\n\t}'
    return {'baseline':source,'branch-clear':source.replace(old,new)}

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='a7dfe334'
    driver.OUT_NAME='playbook-cid-reset-control'
    driver.SOURCE_PATHS=('src/service/cidcore/cid.c',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
