#!/usr/bin/env python3
"""Test loop-carried word outputs, distinct from one terminal publication."""
import playbook_small_patterns as d
import next20_dsp_log_owner as log
import next20_dsp_sqrt_normalize as sqrt

def variants(path,source):
 parent=(log.variants(path,source)['output1-owner1'] if 'log10' in path else sqrt.variants(path,source)['output1-half1-word1'])
 assert parent.count('\t\tint n = 0;')==parent.count('\t\t\tn++;')==parent.count('\t\t*count = (unsigned short)n;')==1
 published=parent.replace('\t\tint n = 0;\n','').replace('\t\t\tn++;','\t\t\t(*count)++;').replace('\t\t*count = (unsigned short)n;\n','')
 return {'baseline':source,'terminal-publication-control':parent,'loop-carried-word-output':published}

if __name__=='__main__':
 d.REV='b470429e';d.SOURCE_PATHS=('src/dsp/fpm_log10.c','src/dsp/fpm_sqrt.c')
 d.OUT_NAME='gcc3-normalization-owner';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
