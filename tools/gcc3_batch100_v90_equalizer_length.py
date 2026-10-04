#!/usr/bin/env python3
import playbook_small_patterns as d
import gcc3_batch100_v90_equalizer_entry as parent
d.OUT_NAME='gcc3-batch100-v90-equalizer-length'
def variants(path,source):
 seed=parent.variants(path,source)['entry-verdict-captured-mode'];cells={'baseline':source,'entry-mode-seed':seed}
 for stem,text in [('independent-length',source),('crossed-length',seed)]:
  for name in ['enterFPE','enterRRN']:
   start,end,fn=d.function(text,'V90Equalizer::'+name)
   fn=fn.replace('\t\tfor (i = 0; i < dfeLength; i++)','\t\tn = dfeLength;\n\t\tfor (i = 0; i < n; i++)',1)
   text=text[:start]+fn+text[end:]
  cells[stem]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
