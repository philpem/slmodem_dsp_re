#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/callprog/Callprog.c',);d.OUT_NAME='gcc3-batch100-callprog-eager-guards'
def variants(path,source):
 cells={'baseline':source}
 for state,timeout,label in [(True,False,'state-eager'),(False,True,'timeout-eager'),(True,True,'both-eager')]:
  text=source
  if state:
   old='if (next == 0 || cp->last_state == next)';assert text.count(old)==1;text=text.replace(old,'if (!((next != 0) & (cp->last_state != next)))')
  if timeout:
   old='if (cp->state != CPSTATE_DIALING\n\t\t    && cp->state != CPSTATE_END_PARTIALLY_STATE)';assert text.count(old)==1;text=text.replace(old,'if ((cp->state != CPSTATE_DIALING)\n\t\t    & (cp->state != CPSTATE_END_PARTIALLY_STATE))')
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
