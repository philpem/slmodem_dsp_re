#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch100-voice-create-converter-owner'
def variants(path,source):
 start,end,fn=d.function(source,'VOICE_create');old='if (!rc_in || !rc_out)';assert fn.count(old)==1;fn=fn.replace(old,'if (!v->rc_in || !v->rc_out)')
 return {'baseline':source,'memory-owned-converter-pair':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
