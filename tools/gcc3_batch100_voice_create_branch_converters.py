#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch100-voice-create-branch-converters'
def variants(path,source):
 start,end,fn=d.function(source,'VOICE_create');old='if (!rc_in || !rc_out)';assert fn.count(old)==1;owner=fn.replace(old,'if (!v->rc_in || !v->rc_out)')
 direct=owner.replace('\tstruct rc *rc_in, *rc_out;\n','')
 for name in ('rc_in','rc_out'):
  line='\t\t'+name+' = 0;\n';assert direct.count(line)==1;direct=direct.replace(line,'')
  line='\t\tv->'+name+' = '+name+';\n';assert direct.count(line)==1;direct=direct.replace(line,'\t\telse\n\t\t\tv->'+name+' = 0;\n')
  direct=direct.replace('\t\t\t'+name+' =','\t\t\tv->'+name+' =')
 return {'baseline':source,'late-owner-read-control':source[:start]+owner+source[end:],'branch-member-converters':source[:start]+direct+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
