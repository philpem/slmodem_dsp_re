#!/usr/bin/env python3
"""Cross unsigned pulse timing, callback reloads and original shared increment."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'IsPulseDialerReady')
 for unsigned,reload,common in itertools.product((False,True),repeat=3):
  if not (unsigned or reload or common):continue
  body=fn
  if reload:
   body=body.replace('c->pulse_elapsed = elapsed + PULSE_TICK_MS;','c->pulse_elapsed += PULSE_TICK_MS;')
   # Hook-off diagnostic is after the external callback and reloads remaining.
   marker='dsplibs_debug_printf("call: %d: hook off...\\n",\n\t\t\t\t\t     remaining);'
   assert marker in body
   body=body.replace(marker,marker.replace('remaining);','c->pulse_remaining);'))
  if unsigned:
   body=body.replace('int remaining, elapsed;','int remaining;\n\tunsigned int elapsed;')
  if common:
   a=body.index('\tif (elapsed < c->pulse_break) {');z=body.index('\n\treturn remaining == 0;',a)
   body=body[:a]+'''\tif (elapsed < c->pulse_break && c->pulse_off_hook == 0) {
\t\tc->pulse_off_hook = 1;
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("call: %d: hook on...\\n", remaining);
\t\tmodem_set_param(dp->modem, MDMPRM_HOOK_ON, 1);
\t} else if (elapsed >= c->pulse_break && c->pulse_off_hook != 0) {
\t\tc->pulse_off_hook = 0;
\t\tmodem_set_param(dp->modem, MDMPRM_HOOK_ON, 0);
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("call: %d: hook off...\\n", REMAINING);
\t} else if (elapsed >= c->pulse_break + c->pulse_make) {
\t\tc->pulse_elapsed = 0;
\t\tc->pulse_remaining--;
\t}
\tINCREMENT;
\treturn c->pulse_remaining == 0;
}'''.replace('REMAINING','c->pulse_remaining' if reload else 'remaining').replace('INCREMENT','c->pulse_elapsed += PULSE_TICK_MS' if reload else 'c->pulse_elapsed = elapsed + PULSE_TICK_MS')
  cells['unsigned-%d-reload-%d-common-%d'%(unsigned,reload,common)]=source[:start]+body+source[end:]
 assert len(set(cells.values()))==8
 return cells
if __name__=='__main__':
 d.REV='902f47fa';d.SOURCE_PATHS=('src/call/call.c',);d.OUT_NAME='gcc3-batch50-pulse-ready';d.variants=variants;d.main()
