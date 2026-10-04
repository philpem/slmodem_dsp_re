#!/usr/bin/env python3
"""Test the original break value shared by both unsigned pulse comparisons."""
import playbook_small_patterns as d
from gcc3_batch50_pulse_ready_return import variants as prior

def variants(path,source):
 cells={'baseline':source,'shared-verdict':prior(path,source)['shared-count-verdict']}
 text=cells['shared-verdict'];start,end,fn=d.function(text,'IsPulseDialerReady')
 marker='\telapsed = c->pulse_elapsed;'
 assert marker in fn
 fn=fn.replace('unsigned int elapsed;','unsigned int elapsed, break_time;')
 fn=fn.replace(marker,marker+'\n\tbreak_time = c->pulse_break;')
 fn=fn.replace('elapsed < c->pulse_break','elapsed < break_time').replace('elapsed >= c->pulse_break','elapsed >= break_time')
 cells['shared-break-value']=text[:start]+fn+text[end:]
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/call/call.c',);d.OUT_NAME='gcc3-batch50-pulse-ready-break';d.variants=variants;d.main()
