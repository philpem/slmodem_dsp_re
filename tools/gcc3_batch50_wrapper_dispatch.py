#!/usr/bin/env python3
"""Cross literal rate lookup and witnessed converter failure sequencing."""
import playbook_small_patterns as d
def variants(path,source):
 start,end,fn=d.function(source,'dp_wrapper_create')
 old=fn[fn.index('\tif (host_srate != dp_srate) {'):fn.rindex('\n\treturn w;')]
 sequential="""\tif (host_srate != dp_srate) {
		int mode = dpw_mode_for(host_srate, dp_srate);
		struct rc *converter = 0;

		if (mode >= 0)
			converter = RcFixed_Create(mode);
		w->rc_to_dp = converter;
		if (converter == 0) {
			dp_wrapper_delete(w);
			return NULL;
		}
		mode = dpw_mode_for(dp_srate, host_srate);
		converter = 0;
		if (mode >= 0)
			converter = RcFixed_Create(mode);
		w->rc_to_host = converter;
		if (converter == 0) {
			dp_wrapper_delete(w);
			return NULL;
		}
	}
"""
 cells={}
 for literal in (0,1):
  for seq in (0,1):
   text=source
   if seq:text=source[:start]+fn.replace(old,sequential)+source[end:]
   if literal:
    a=text.index('struct dpw_rate_pair {');z=text.index('\nstruct dp_wrapper *',a)
    helper='static int\ndpw_mode_for(int from, int to)\n{\n'
    for f,t,m in [(8000,9600,2),(9600,8000,3),(8000,48000,4),(48000,8000,5),(9600,48000,6),(48000,9600,7)]:helper+=f'\tif (from == {f} && to == {t})\n\t\treturn {m};\n'
    helper+='\treturn -1;\n}\n'
    text=text[:a]+helper+text[z:]
   label='baseline' if not(literal or seq) else f'literal-{literal}-sequential-{seq}'
   cells[label]=text
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/core/dp_wrapper.c',);d.OUT_NAME='gcc3-batch50-wrapper-dispatch';d.variants=variants;d.main()
