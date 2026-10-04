#!/usr/bin/env python3
"""Cross loop-carried output with conventional top-tested helper loop."""
import playbook_small_patterns as d
import gcc3_normalization_owner_reproduce as owner

def variants(path,source):
 prior=owner.variants(path,source);cells={'baseline':source}
 for pointer,label in [(False,'terminal'),(True,'word-output')]:
  text=prior['loop-carried-word-output' if pointer else 'terminal-publication-control']
  cells[label+'-guard-do']=text
  threshold='0x3fff' if 'log10' in path else '0x1fffffffu'
  a=text.index('\nnormalize16(' if 'log10' in path else '\nnormalize_sqrt32(')+1;z=text.index('\n}\n',a)+2
  fn=text[a:z]
  increment='value = (unsigned short)(value << 1);' if 'log10' in path else 'value += value;'
  if pointer:
   old='\tif (value <= '+threshold+') {\n\t\tdo {\n\t\t\t'+increment+'\n\t\t\t(*count)++;\n\t\t} while (value <= '+threshold+');\n\t}'
   new='\twhile (value <= '+threshold+') {\n\t\t'+increment+'\n\t\t(*count)++;\n\t}'
  else:
   old='\tif (value <= '+threshold+') {\n\t\tint n = 0;\n\t\tdo {\n\t\t\t'+increment+'\n\t\t\tn++;\n\t\t} while (value <= '+threshold+');\n\t\t*count = (unsigned short)n;\n\t}'
   new='\tint n = 0;\n\twhile (value <= '+threshold+') {\n\t\t'+increment+'\n\t\tn++;\n\t}\n\t*count = (unsigned short)n;'
  assert fn.count(old)==1,(path,old)
  cells[label+'-while']=text[:a]+fn.replace(old,new)+text[z:]
 return cells

if __name__=='__main__':
 d.REV='b470429e';d.SOURCE_PATHS=('src/dsp/fpm_log10.c','src/dsp/fpm_sqrt.c');d.OUT_NAME='gcc3-normalization-loop';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
