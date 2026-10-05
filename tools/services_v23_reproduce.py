#!/usr/bin/env python3
"""V23 transmit loop, mute-tail and captured-bit boundary cross."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for stream,mute,unset in itertools.product((False,True),repeat=3):
  if not (stream or mute or unset):continue
  t=source
  if stream:t=t.replace('while (count > 0)', 'while (count != 0)')
  if unset:t=t.replace('int bit = 0;', 'int bit;')
  if mute:
   start=t.index('\tif (tx->mute) {',t.index('v23FP_tx_progress('))
   end=t.index('\n\twhile (count',start)
   t=t[:start]+'''\tif (tx->mute) {
		for (done = 0; done < count; done++)
			*out++ = 0;
		done = count;
		tx->mute = 0;
		goto report_count;
	}
'''+t[end:]
   t=t.replace('\n\t*consumed = done;','\nreport_count:\n\t*consumed = done;')
  cells[f'zero-{int(stream)}-mute-{int(mute)}-unset-{int(unset)}']=t
 assert len(cells)==len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/pump/v23/v23tx.c',);d.OUT_NAME='services-v23'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
