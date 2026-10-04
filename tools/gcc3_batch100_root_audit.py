#!/usr/bin/env python3
"""Audit bounded root source controls, complete data and nonexact bystanders."""
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
b=d.b
s=(d.ROOT/'tools/playbook_bwch_unit_audit.py').read_text();exec(s[s.index('def inspect'):s.index('reports=')])
s=(d.ROOT/'tools/gcc3_batch20_root_audit.py').read_text();exec(s[s.index('def allocated'):s.index('\nknown=')])
specs={'fdsp-conversion-helpers':(4,{'FDSP_DP_Run'}),'fdsp-helper-predecessor':(4,{'FDSP_DP_Run','create_dtmf'}),'energy-lifetime':(4,{'bSearchEnergy','FindCorrelation','FDSP_DP_Run'}),'energy-counter-mean':(5,{'bSearchEnergy','FindCorrelation','FDSP_DP_Run'}),'dtmf-create-return':(2,{'create_dtmf'}),'beepgen-return':(2,{'beepgen_create','beepgen_sample','beepgen_start_beep','beepgen_start_dtmf'}),'voice-dle':(3,{'voice_dle_command'}),'cross-links':(4,{'CrossDataLinks'}),'dialer-verdict':(4,{'IsDialStringInvalid','DialerCreate'}),'rx-query':(5,{'voice_set_rx','voice_rx'}),'voice-count':(5,{'voice_modem'}),'voice-online':(4,{'voice_online'}),'voice-online-gate':(4,{'voice_online'}),'stability-result':(4,{'check_for_valid','check_for_valid_easy'}),'stability-width':(6,{'check_for_valid','check_for_valid_easy'})}
reports={};total=0
for key,(count,allowed) in specs.items():
 root=d.ROOT/'build'/('gcc3-batch100-'+key);families=json.loads((root/'results.json').read_text())['families']
 for family,f in families.items():
  cells=f['cells'];assert len(cells)==count and cells['baseline']['baseline_reproduced']
  base=root/family/'baseline/candidate.o';meta=inspect(base);data=allocated(base)
  for label,c in cells.items():
   p=root/family/label/'candidate.o';assert inspect(p)==meta,(key,label,'metadata/data')
   actual=allocated(p)
   assert set(actual)==set(data),(key,label,'allocated section inventory')
   for sec,value in actual.items():
    if value==data[sec]:continue
    assert sec.startswith('.rodata.str'),(key,label,'nontext difference',sec)
    with p.open('rb') as stream:
     section=ELFFile(stream).get_section_by_name(sec)
     assert section['sh_flags']&0x30==0x30 and section['sh_entsize']==1
    assert value['relocations']==data[sec]['relocations']=={}
    assert sorted(bytes.fromhex(value['bytes']).split(b'\0'))==sorted(bytes.fromhex(data[sec]['bytes']).split(b'\0')),(key,label,'literal pool values')
   assert set(c.get('changed_bodies',[]))<=allowed,(key,label,'bystanders')
   assert not c.get('losses'),(key,label,'exact loss')
   reports[key+'/'+label]={'functions':len(c['functions']),'changed':c.get('changed_bodies',[]),'gains':c.get('gains',[])};total+=1
assert total==65,total
winner=d.ROOT/'build/gcc3-batch100-voice-dle/voice/common-switch/candidate.o'
assert b.verdict(*b.body(b.BLOB,'voice_dle_command'),*b.body(str(winner),'voice_dle_command'))==('EXACT',0)
assert reports['rx-query/owner-every-use']['changed']==['voice_set_rx']
assert reports['rx-query/late-1-direct-1']['changed']==['voice_set_rx']
rxwinner=d.ROOT/'build/gcc3-batch100-rx-query/Rx/owner-every-use/candidate.o'
assert b.verdict(*b.body(b.BLOB,'voice_set_rx'),*b.body(str(rxwinner),'voice_set_rx'))==('EXACT',0)
(d.ROOT/'build/batch100-root-complete-audit.json').write_text(json.dumps({'valid_tus':total,'reports':reports},indent=2)+'\n')
print('65 complete TUs: unchanged metadata/named data/relocations; reviewed merge-string permutation; all bystanders bounded, zero losses; original196B DLE positive control fires')
