#!/usr/bin/env python3
"""Original tone result width crossed with escaped phase updates."""
import sys,subprocess,itertools
from pathlib import Path
import playbook_small_patterns as d
def parts(label):
 if label=='baseline':return 'void',False
 return label.split('-')[1],bool(int(label.split('-')[3]))
def overlays(path,label):
 typ,phase=parts(label)
 if typ=='void':return {}
 h=subprocess.check_output(['git','show','902f47fa:include/dsplib/fdspkrnl.h'],cwd=d.ROOT,text=True)
 assert h.count('void TONE_generate(')==1
 return {'dsplib/fdspkrnl.h':h.replace('void TONE_generate(',typ+' TONE_generate(')}
def variants(path,source):
 a,z,fn=d.function(source,'TONE_generate');f=fn.replace('return;','return n;');f=f[:-1]+'\treturn n;\n}'
 seed=source[:a]+f+source[z:];seed=seed.replace('\nvoid\nTONE_generate(','\nshort\nTONE_generate(')
 keep='if (t->duration > tm || 0.0f >= t->duration) {\n\t\tt->elapsed = tm;\n\t\tt->phase = ph.phase;\n\t\treturn n;\n\t}\n\t'
 assert keep in f
 q=f.replace(keep,'if (t->duration <= tm && t->duration > 0.0f) {\n\t')
 q=q.replace('t->phase = ph.phase;\n\treturn n;','t->phase = ph.phase;\n\t} else {\n\t\tt->elapsed = tm;\n\t\tt->phase = ph.phase;\n\t}\n\treturn n;')
 candidate=(source[:a]+q+source[z:]).replace('\nvoid\nTONE_generate(','\nshort\nTONE_generate(')
 return {'baseline':source,'result-short-phase-0':seed,'result-short-phase-2':candidate}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-service-tone-duration';d.SOURCE_PATHS=('src/service/TONE.c',);d.HEADER_OVERLAYS=overlays;d.variants=variants;d.main()
