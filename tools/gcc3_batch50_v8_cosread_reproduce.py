#!/usr/bin/env python3
"""V8 cosine formal width x caller casts, low byte narrowed at owner use."""
import sys,subprocess,itertools
from pathlib import Path
import playbook_small_patterns as d
FORMALS={'byte':'unsigned char','signed':'int','unsigned':'unsigned int'}

def parts(label):
 if label=='baseline':return 'byte',0
 fields=label.split('-');return fields[1],int(fields[3])
def overlays(path,label):
 kind,remove=parts(label)
 if kind=='byte':return {}
 text=subprocess.check_output(['git','show','902f47fa:include/dsplib/v8.h'],cwd=d.ROOT,text=True)
 assert text.count('short v8_cosread(unsigned char phase);')==1
 return {'dsplib/v8.h':text.replace('short v8_cosread(unsigned char phase);','short v8_cosread('+FORMALS[kind]+' phase);')}
def variants(path,source):
 cells={}
 for kind,remove in itertools.product(FORMALS,(0,1)):
  label='baseline' if kind=='byte' and not remove else f'formal-{kind}-casts-{remove}'
  text=source
  if kind!='byte' and path.endswith('V8Dftc.c'):
   a,z,fn=d.function(text,'v8_cosread');fn=fn.replace('unsigned char phase',FORMALS[kind]+' phase').replace('v8_costbl[phase]','v8_costbl[(unsigned char)phase]');text=text[:a]+fn+text[z:]
  if remove:
   # Remove only byte conversions inside cosine-call arguments, using balanced parentheses.
   cursor=0
   while True:
    start=text.find('v8_cosread(',cursor)
    if start<0:break
    arg=start+len('v8_cosread(');end=arg;depth=1
    while depth:
     if text[end]=='(':depth+=1
     elif text[end]==')':depth-=1
     end+=1
    before=text[arg:end-1];after=before.replace('(unsigned char)','')
    text=text[:arg]+after+text[end-1:];cursor=arg+len(after)+1
  cells[label]=text
 assert len(cells)==6
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v8-cosread';d.SOURCE_PATHS=('src/v8/V8.c','src/v8/V8Dftc.c','src/v8/V8Fsk.c');d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
