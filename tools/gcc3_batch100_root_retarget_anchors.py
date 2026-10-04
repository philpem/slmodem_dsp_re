#!/usr/bin/env python3
"""Maintain existing DLE fault anchors after common-result recovery; no execution."""
import playbook_small_patterns as d
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'test/mutations/vcedle.json';rows=json.loads(p.read_text());labels=[r.get('label') for r in rows]
a='\t\tv->dle_etx = 1;\n\t\tbreak;'
u='\t\t\tdsplibs_debug_printf("Unknown command - %2x\\n", cmd);'
updates={
 'the ETX flag is set to 2':(a,a.replace('= 1','= 2')),
 'ETX returns 1':(a,a.replace('\t\tbreak;','\t\tstatus = 1;\n\t\tbreak;')),
 'CAN returns 8':('\t\tstatus = VOICE_DLE_CAN_STATUS;','\t\tstatus = VOICE_DLE_CAN_STATUS - 1;'),
 'an unknown command returns 9':(u+'\n\t}\n\treturn status;',u+'\n\t\tstatus = VOICE_DLE_CAN_STATUS;\n\t}\n\treturn status;'),
 'the unknown-command gate fires at level 1':('\t\tif (DSPLIB_DEBUG_ON())\n'+u,'\t\tif (dsplibs_debug_level > 0)\n'+u),
 'the ETX arm falls through to CAN':(a+'\n\tcase VOICE_DLE_CAN:',a.replace('\t\tbreak;','\t\t/* fallthrough */')+'\n\tcase VOICE_DLE_CAN:')}
proof=[]
for row in rows:
 if row.get('label') in updates:
  row['find'],row['replace']=updates[row['label']];proof.append(row['label'])
assert [r.get('label') for r in rows]==labels and len(proof)==6
p.write_text(json.dumps(rows,indent=1)+'\n')
q=ROOT/'test/mutations/voicedprx.json';rxrows=json.loads(q.read_text())
for row in rxrows:
 if row.get('label')=='voice_set_rx hands the silence detector the wrong handle':
  row['find']='\tsilence_create(v->silence, v->cfg.modem,\n\t    (unsigned int (*)(void *, int))v->cfg.get_sreg);'
  row['replace']=row['find'].replace('v->cfg.modem,','v->silence,');proof.append(row['label'])
for row in rxrows:
 if row.get('label')=="the fmt1 and fmt3 gains are read from each other's parameter":
  row['find']='\tv->gain_fmt1 = (float)(unsigned int)v->cfg.get_sreg(v->cfg.modem, VOICE_PARAM_RX_GAIN_FMT1)\n\t\t       * VOICE_RX_PARAM_SCALE;'
  row['replace']=row['find'].replace('RX_GAIN_FMT1)','RX_GAIN_FMT3)');proof.append(row['label'])
 if row.get('label')=="the host's answer is read as signed":
  row['find']='\tv->gain_other = (float)(unsigned int)v->cfg.get_sreg(v->cfg.modem, VOICE_PARAM_RX_GAIN_OTHER)\n\t\t\t* VOICE_RX_PARAM_SCALE;'
  row['replace']=row['find'].replace('(unsigned int)','(int)');proof.append(row['label'])
q.write_text(json.dumps(rxrows,indent=1)+'\n')
suites=json.loads((ROOT/'test/mutations/suites.json').read_text());checked=0
for suite in ('vcedle','voicedprx'):
 source=(ROOT/suites[suite][0]).read_text()
 for row in json.loads((ROOT/'test/mutations'/ (suite+'.json')).read_text()):
  if row.get('skip') or 'find' not in row:continue
  assert source.count(row['find'])==1,(suite,row['label'],source.count(row['find']))
  assert row['replace']!=row['find'];checked+=1
(ROOT/'build/batch100-root-anchor-retarget.json').write_text(json.dumps({'retargeted':proof,'checked':checked,'detached':0,'nonunique':0},indent=2)+'\n')
print('9 retargeted existing faults;',checked,'static anchors checked; 0 detached/nonunique; no mutants executed')
