#!/usr/bin/env python3
"""Static, suite-scoped retargeting of existing batch50 mutation anchors."""
import json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
suites=('v90adid','v90modprog','v90p3mod','v90prefilter','v90rdet','v90unpck','v92p3mod')
registry=json.loads((ROOT/'test/mutations/suites.json').read_text());updates=[]
for suite in suites:
 path=ROOT/'test/mutations'/f'{suite}.json';rows=json.loads(path.read_text());src=(ROOT/registry[suite][0]).read_text();oldrows=json.loads(path.read_text())
 for i,row in enumerate(rows):
  if 'find' not in row or src.count(row['find'])==1:continue
  if suite=='v90adid' and i==46:
   row['find']='\tif (sum > 0)\n\t\tresult = 1;\n\treturn result;'
   row['replace']=row['find'].replace('sum > 0','sum != 0')
  elif suite=='v90modprog' and i==101:
   start=src.index('V90Modulator::enterDataPhase()');end=src.index('\n\tedprintf(',start)
   row['find']=src[start:end];row['replace']=row['find'].replace('\n\tif (state == 3)\n\t\treturn;','')
  elif suite=='v90p3mod':
   row['find']=row['find'].replace('return generate','return (short)generate')
   row['replace']=row['replace'].replace('return generate','return (short)generate')
  elif suite=='v90prefilter' and i==12:
   row['find']='\tint n = 0;\n\tV90RefLoop *loops = dataBase[codecType].loops;'
   row['replace']=row['find'].replace('dataBase[codecType]','dataBase[0]')
  elif suite=='v90unpck':
   ordinary='codecConstellation' not in row['find'];table='constellation' if ordinary else 'codecConstellation'
   row['find']='\tunsigned int c = (unsigned int)which;\n\tif (which >= 6) c = 0;\n\tunsigned char *table = params->'+table+'[c];'
   row['replace']=row['find'].replace('which >= 6','which >= 3' if i==24 else 'which > 6')
  elif suite=='v92p3mod':
   for key in ('find','replace'):
    row[key]=row[key].replace('Modulator *m)\n{\n\tswitch','Modulator *m)\n{\n\tint sample;\n\n\tswitch')
    row[key]=re.sub(r'\t\treturn ([^;]+);',r'\t\tsample = \1;\n\t\tbreak;',row[key])
    if '\tint sample;\n\n\tswitch' not in src:row[key]=row[key].replace('\tint sample;\n\n\tswitch','\tint sample;\n\tswitch')
  elif suite=='v90rdet':
   if i in (13,14):
    start=src.index('V90RDetector::detectR(short sample)');end=src.index('\n\ttaken =',start)
    row['find']=src[start:end]
    row['replace']=row['find'].replace('sample > 0','sample >= 0') if i==13 else row['find'].replace('signBits <<= 1','signBits >>= 1')
   elif i==15:row['find']='\tif (taken == 6) {';row['replace']='\tif (taken == 5) {'
   elif i==19:
    row['find']='\t\t\tif (negativeRunLength == rLimit) {\n\t\t\t\tpolarity = -1;';row['replace']=row['find'].replace('polarity = -1','polarity = 1')
   elif i==20:
    row['find']='\t\t} else {\n\t\t\tpositiveRunLength = 0;\n\t\t\tnegativeRunLength = 0;\n\t\t}\n\n\t\tsampleCount = 0;\n\t\tsignBits = 0;\n\t} else {\n\t\tsampleCount = taken;'
    row['replace']=row['find'].replace('\n\t\t\tnegativeRunLength = 0;','')
   elif i==21:
    row['find']='\t\tsampleCount = 0;\n\t\tsignBits = 0;\n\t} else {\n\t\tsampleCount = taken;';row['replace']=row['find'].replace('\n\t\tsignBits = 0;','')
   elif i==26:
    row['find']='\t\t} else {\n\t\t\tnotRunLength = 0;\n\t\t}\n\n\t\tsampleCount = 0;\n\t\tsignBits = 0;\n\t} else {\n\t\tsampleCount = used;'
    row['replace']=row['find'].replace('\t\t} else {\n\t\t\tnotRunLength = 0;\n\t\t}','\t\t}')
   elif i==27:row['find']='\tif (seen == 12) {';row['replace']='\tif (seen == 6) {'
   else:raise AssertionError((suite,i))
  else:raise AssertionError((suite,i))
  assert src.count(row['find'])==1,(suite,i,src.count(row['find']))
  assert {k:v for k,v in row.items() if k not in ('find','replace')}=={k:v for k,v in oldrows[i].items() if k not in ('find','replace')}
  updates.append({'suite':suite,'index':i,'source':registry[suite][0],'before':oldrows[i],'after':row,'matches':1})
 assert len(rows)==len(oldrows)
 assert all('find' not in row or src.count(row['find'])==1 for row in rows),suite
 path.write_text(json.dumps(rows,indent=1)+'\n')
(ROOT/'build/batch50-v90-anchor-retarget.json').write_text(json.dumps({'updated':len(updates),'rows':updates},indent=2)+'\n')
print(len(updates),'existing rows retargeted, seven suite counts/labels/metadata preserved; every find matches once')
