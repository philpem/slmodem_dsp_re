#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch100-voice-command-duration-result'
def variants(path,source):
 start,end,fn=d.function(source,'VOICE_command')
 old='\t\tdur = info->tone_duration / 10;\n\t\tif (dur == 0)\n\t\t\tdur = 1;';assert fn.count(old)==1
 new='\t\t{\n\t\t\tunsigned int quotient = info->tone_duration / 10;\n\n\t\t\tdur = 1;\n\t\t\tif (quotient != 0)\n\t\t\t\tdur = quotient;\n\t\t}'
 fn=fn.replace(old,new)
 return {'baseline':source,'default-result-first':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
