#!/usr/bin/env python3
"""CID reset final publication relative to the automatic-mode predicate."""
import playbook_small_patterns as d

def variants(path,source):
 old='\tctx->samples_fill = 0;\n\tif (ctx->mode == CID_MODE_AUTOMATIC)\n\t\tctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;'
 assert source.count(old)==1
 split='''	if (ctx->mode == CID_MODE_AUTOMATIC) {
		ctx->samples_fill = 0;
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
	} else {
		ctx->samples_fill = 0;
	}'''
 after='''	if (ctx->mode == CID_MODE_AUTOMATIC)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
	ctx->samples_fill = 0;'''
 return {'baseline':source,'predicate-first-split':source.replace(old,split),'common-after':source.replace(old,after)}
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/service/cidcore/cid.c',);d.OUT_NAME='services-cid-reset'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
