#!/usr/bin/env python3
"""V32 phase reversal persistent count and arm-specific RTT publication."""
import itertools
import playbook_small_patterns as d

def counter(body):
 old='''		if (FPM_TONE_detect(hdx->tone0, in,
				    (short)*count) == 0)
			miss = (short)(HDX(modem)->short_ac
				       + 1);
		else
			miss = 0;

		hdx = HDX(modem);
		hdx->short_ac = miss;'''
 new='''		if (FPM_TONE_detect(hdx->tone0, in,
				    (short)*count) == 0)
			HDX(modem)->short_ac++;
		else
			HDX(modem)->short_ac = 0;

		hdx = HDX(modem);'''
 assert body.count(old)==1
 return body.replace(old,new).replace('\tshort miss;\n','')

def rtt(body):
 body=body.replace('\t\tint scaled;\n\n','')
 old='''			hdx = HDX(modem);
			scaled = ((int)rev * V32_REV_SCALE + V32_REV_ROUND)
				 >> V32_REV_SHIFT;

			if (hdx->mode == V32_MODE_ORIGINATE) {
				hdx->rtd = (short)
					(scaled - 2 * hdx->turnaround);
			} else {
				hdx->rtd = (short)
					(scaled
					 + hdx->symbol_len
					 - hdx->turnaround
					 - hdx->short_9c
					 - hdx->short_98
					 - hdx->short_9a);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("v32 RTD = %d\\n",
						(int)hdx->rtd);
				hdx = HDX(modem);
			}

			if (hdx->rtd < 0)
				hdx->rtd = 0;'''
 new='''			hdx = HDX(modem);
			if (hdx->mode == V32_MODE_ORIGINATE) {
				short delay = (short)
					((((int)rev * V32_REV_SCALE + V32_REV_ROUND)
					  >> V32_REV_SHIFT) - 2 * hdx->turnaround);
				if (delay < 0)
					delay = 0;
				hdx->rtd = delay;
			} else {
				hdx->rtd = (short)
					((((int)rev * V32_REV_SCALE + V32_REV_ROUND)
					  >> V32_REV_SHIFT)
					 + hdx->symbol_len
					 - hdx->turnaround
					 - hdx->short_9c
					 - hdx->short_98
					 - hdx->short_9a);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("v32 RTD = %d\\n",
						(int)hdx->rtd);
				hdx = HDX(modem);
				if (hdx->rtd < 0)
					hdx->rtd = 0;
			}'''
 assert body.count(old)==1
 return body.replace(old,new)

def variants(path,source):
 cells={'baseline':source}
 for persistent,publication in itertools.product((False,True),repeat=2):
  if not(persistent or publication):continue
  a,b,body=d.function(source,'RxHdxPhsReversal')
  if persistent:body=counter(body)
  if publication:body=rtt(body)
  cells[f'persistent-{int(persistent)}-publication-{int(publication)}']=source[:a]+body+source[b:]
 return cells
if __name__=='__main__':
 d.REV='6b4509bd';d.SOURCE_PATHS=('src/pump/v32/V32rxhdx.c',);d.OUT_NAME='data-state-reversal'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
