#!/usr/bin/env python3
"""Retarget existing fault anchors after source recovery; no execution."""
import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]; touched=[]
p=root/'test/mutations/beepgen.json';rows=json.loads(subprocess.check_output(['git','show','856c1ecb:'+str(p.relative_to(root))],cwd=root,text=True))
for i in (60,61):
 for k in ('find','replace'):rows[i][k]='\n'.join('\t'+line if line else line for line in rows[i][k].split('\n'))
 touched.append(('beepgen',i))
p.write_text(json.dumps(rows,indent=1)+'\n')
p=root/'test/mutations/v90modprog.json';rows=json.loads(subprocess.check_output(['git','show','856c1ecb:'+str(p.relative_to(root))],cwd=root,text=True));src=(root/'src/pump/v90/V90Modulator.cpp').read_text()
rrn=src[src.index('\tif (bitsToSymbol->nofBitsForNextTime() != 0)',src.index('V90Modulator::initiateRRN()')):];rrn=rrn[:rrn.index('\n\t}\n')+4]
rows[50]['find']=rrn
rows[50]['replace']=rrn.replace('"RdModulation','"TMP').replace('"DataToRdModulation','"RdModulation').replace('"TMP','"DataToRdModulation')
prefix='\t\t\t "DataToRdModulation\\r\\n");\n\t}\n\n\tphase4Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,\n'
rows[52]['find']=prefix;rows[52]['replace']=prefix.replace('phase2Info->Uinfo','0')
rows[53]['find']=prefix+'\t\t\t       p4state, 0, phase2Info->rtd);\n';rows[53]['replace']=rows[53]['find'].replace('p4state, 0,','p4state, 1,')
for i in (116,117,120):
 for k in ('find','replace'):
  rows[i][k]=rows[i][k].replace('\n\tif (bitsToSymbol->nofBitsForNextTime() != 0) {\n','\n\tif (bitsToSymbol->nofBitsForNextTime() != 0) {\n\t\tp4state = P4M_STATE_RF;\n').replace('\n\tif (bitsToSymbol->nofBitsForNextTime() == 0) {\n','\n\tif (bitsToSymbol->nofBitsForNextTime() == 0) {\n\t\tp4state = P4M_STATE_RF;\n')
fpe=src[src.index('\tif (bitsToSymbol->nofBitsForNextTime() != 0)',src.index('V90Modulator::initiateFPE()')):];fpe=fpe[:fpe.index('\n\t}\n')+4]
rows[118]['find']=fpe;rows[118]['replace']=fpe.replace('P4M_STATE_RF','P4M_STATE_TMP').replace('P4M_STATE_UNNAMED_1C','P4M_STATE_RF').replace('P4M_STATE_TMP','P4M_STATE_UNNAMED_1C')
for i in (50,52,53,116,117,118,120):touched.append(('v90modprog',i))
p.write_text(json.dumps(rows,indent=1)+'\n')
suites=json.loads((root/'test/mutations/suites.json').read_text());count=0
for suite in ('beepgen','v90modprog'):
 source=(root/suites[suite][0]).read_text()
 for row in json.loads((root/'test/mutations'/ (suite+'.json')).read_text()):
  if row.get('skip') or 'find' not in row:continue
  assert source.count(row['find'])==1,(suite,row['label'],source.count(row['find']))
  assert row['replace']!=row['find'];count+=1
print('retargeted',len(touched),'existing faults;',count,'static anchors unique; no mutation execution')
(root/'build/batch100-final-anchor-retarget.json').write_text(json.dumps({'retargeted':touched,'checked':count},indent=2)+'\n')
