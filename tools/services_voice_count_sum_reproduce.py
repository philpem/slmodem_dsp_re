#!/usr/bin/env python3
"""Voice mixed count operands at the final output publication boundary."""
import playbook_small_patterns as d

def variants(path,source):
 old='\t*countp = saved + det_len;';assert source.count(old)==1
 return {'baseline':source,'detector-plus-handler':source.replace(old,'\t*countp = det_len + saved;')}
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/voice/voice.c',);d.OUT_NAME='services-voice-count-sum'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
