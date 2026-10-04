#!/usr/bin/env python3
"""Move common zero result to its original post-callback boundary."""
import playbook_small_patterns as d
from gcc3_batch50_voice_register import variants as prior
def variants(path,source):
 cells={'baseline':source}
 for label,text in prior(path,source).items():
  if label=='baseline':continue
  text=text.replace('unsigned int result = 0;','unsigned int result;')
  marker='vi = (struct voice_info *)modem_get_param(modem, MDMPRM_VOICEINFO);'
  assert text.count(marker)==1
  text=text.replace(marker,marker+'\n\tresult = 0;')
  cells[label+'-after-call']=text
 assert len(set(cells.values()))==3
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch50-voice-register-lifetime';d.variants=variants;d.main()
