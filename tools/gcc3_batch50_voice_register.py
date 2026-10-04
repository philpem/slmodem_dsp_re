#!/usr/bin/env python3
"""Original voice getter common result and guarded low sensitivity."""
import playbook_small_patterns as d
def variants(path,source):
 a,z,fn=d.function(source,'vce_get_sreg');head=fn[:fn.index('{')+1]
 cells={'baseline':source}
 for branch in (0,1):
  sensitivity="""		result = vi->silence_detect_sensitivity >> VCE_SILENCE_LEVEL_SHIFT;
		if (result == 0) {
			if (vi->silence_detect_sensitivity != 0)
				result = 1;
		} else if (result > VCE_SILENCE_LEVEL_MAX)
			result = VCE_SILENCE_LEVEL_MAX;
""" if branch else """		result = vi->silence_detect_sensitivity >> VCE_SILENCE_LEVEL_SHIFT;
		if (result == 0)
			result = vi->silence_detect_sensitivity != 0;
		else if (result > VCE_SILENCE_LEVEL_MAX)
			result = VCE_SILENCE_LEVEL_MAX;
"""
  body=head+"""
	struct voice_info *vi;
	unsigned int result = 0;

	vi = (struct voice_info *)modem_get_param(modem, MDMPRM_VOICEINFO);

	switch (num) {
	case SREG_FLASH_TIMER:
		result = VCE_FLASH_TIMER;
		break;
	case SREG_HANDSET_GANE:
		result = VCE_HANDSET_GAIN;
		break;
	case SREG_VOICE_DIALTONE_DETECT_DELAY:
		result = VCE_DIALTONE_DETECT_DELAY;
		break;
	case SREG_SILENCE_DETECT_SENSITIVITY:
"""+sensitivity+"""		break;
	case SREG_SILENCE_DETECT_DURATION:
		result = vi->silence_detect_period;
		break;
	case SREG_MIC_GAIN:
	case SREG_LINE_RECORD_GAIN:
		result = vi->rx_gain;
		break;
	}
	return result;
}"""
  cells['common-guarded' if branch else 'common-boolean']=source[:a]+body+source[z:]
 assert len(set(cells.values()))==3
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/voice.c',);d.OUT_NAME='gcc3-batch50-voice-register';d.variants=variants;d.main()
