#!/usr/bin/env python3
"""V32 shared timer extension and narrow report acquisition boundaries."""
import itertools
import playbook_small_patterns as d
import data_state_reversal_reproduce as prior

def plain_counter(t):
 old='''	hdx->short_a8 = (unsigned short)
		(hdx->short_a8
		 + hdx->symbol_len);'''
 assert t.count(old)==1
 return t.replace(old,'\thdx->short_a8 += hdx->symbol_len;')

def report(t):
 needle='''			} else {
				hdx->rtd = (short)''';assert t.count(needle)==1
 t=t.replace(needle,'''\t\t\t} else {
				short measured;

				measured = (short)''')
 needle='''					 - hdx->short_9a);
				if (DSPLIB_DEBUG_ON())''';assert t.count(needle)==1
 t=t.replace(needle,'''\t\t\t\t\t - hdx->short_9a);
				hdx->rtd = measured;
				if (DSPLIB_DEBUG_ON())''')
 needle='"v32 RTD = %d\\n",\n\t\t\t\t\t\t(int)hdx->rtd';assert t.count(needle)==1
 return t.replace(needle,'"v32 RTD = %d\\n",\n\t\t\t\t\t\t(int)measured')

def variants(path,source):
 seeds=prior.variants(path,source)
 cells={'baseline':source}
 for graph,shared,measured in itertools.product((False,True),repeat=3):
  if not(graph or shared or measured):continue
  t=seeds['persistent-1-publication-1'] if graph else source
  if shared:t=plain_counter(t)
  if measured:t=report(t)
  cells[f'graph-{int(graph)}-shared-{int(shared)}-report-{int(measured)}']=t
 return cells
if __name__=='__main__':
 d.REV='6b4509bd';d.SOURCE_PATHS=('src/pump/v32/V32rxhdx.c',);d.OUT_NAME='data-state-reversal-width'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
