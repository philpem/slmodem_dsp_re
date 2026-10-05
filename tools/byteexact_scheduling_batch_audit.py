#!/usr/bin/env python3
"""Require the combined ten-function batch to reproduce independent winners."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

WINNERS={
 'src_service_cidcore_cid.c.o':('services-cid-reset/cid/common-after/candidate.o',{'cid_reset','cid_create'}),
 'src_pump_v90_V90bitsToSymbol.cpp.o':('batch-cpp-bitreset/V90bitsToSymbol/conditional/candidate.o',{'_ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType'}),
 **{'src_fax_Vmi_v'+mode+'.c.o':('fax-adapter-consumers/Vmi_v'+mode+'/modem-status/candidate.o',{'v'+mode+'rx_process','v'+mode+'tx_process'}) for mode in ('17','21','27','29')}
}
GAINS={'cid_reset','_ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType',
       *('v'+mode+side+'_process' for mode in ('17','21','27','29') for side in ('rx','tx'))}


def main():
 before=d.ROOT/'build/production-before';after=d.ROOT/'build/tc_out'
 names={p.name for p in before.glob('*.o')}
 assert len(names)==300 and names=={p.name for p in after.glob('*.o')}
 assert (before/'.build-config').read_bytes()==(after/'.build-config').read_bytes()
 changed=[n for n in sorted(names) if (before/n).read_bytes()!=(after/n).read_bytes()]
 assert set(changed)==set(WINNERS),changed
 rows=[]
 for name,(winner,expected) in WINNERS.items():
  p=after/name
  assert p.read_bytes()==(d.ROOT/'build'/winner).read_bytes(),(name,'winner raw repeat')
  a,b=inspect(before/name),inspect(p)
  for key in ('records','allocated','nobits','relocations'):
   assert a[key]==b[key],(name,key)
  assert set(a['text_positions'])==set(b['text_positions'])
  bodies=[n for n in a['text_positions'] if d.b.body(str(before/name),n)!=d.b.body(str(p),n)]
  assert set(bodies)==expected,(name,bodies)
  rows.append({'object':name,'changed_bodies':sorted(bodies),'winner_raw_equal':True})
 old=json.loads((d.ROOT/'build/baseline-byteident.json').read_text())
 new=json.loads((d.ROOT/'build/batch-final-byteident.json').read_text())
 gains=set(new['exact_symbols'])-set(old['exact_symbols']);losses=set(old['exact_symbols'])-set(new['exact_symbols'])
 assert gains==GAINS and not losses,(gains,losses)
 assert new['compared']==old['compared']==1852
 assert new['exact']==old['exact']+10==1067
 assert new['exact_bytes']==old['exact_bytes']+722==115564
 report={'objects':300,'raw_unchanged':294,'changed_objects':rows,
         'configuration_equal':True,'gains':sorted(gains),'losses':sorted(losses),
         'exact':new['exact'],'compared':new['compared'],'exact_bytes':new['exact_bytes'],
         'gain_original_bytes':722,'changed_body_count':11}
 (d.ROOT/'build/batch-production-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Production audit: 300 objects, 294 raw unchanged, 6 raw winner repeats; '
       '10 gains, 0 losses, 1067/1852 exact, +722 original bytes; 11 changed bodies')


if __name__=='__main__':main()
