#!/usr/bin/env python3
"""Audit completed SineWave entry controls; width proposals are not executable."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
TARGET='_ZN8SineWaveIffE8generateEPfm'
def main():
 root=d.ROOT/'build/batch-cpp-sine-entry';fr=json.loads((root/'results.json').read_text())['families']['VpcmFloModem'];folder=root/'VpcmFloModem';base=folder/'baseline/candidate.o';assert base.read_bytes()==(folder/'retained.o').read_bytes();bi=inspect(base);report={};total=0
 assert set(fr['cells'])=={'baseline','common-loop'}
 for label,cell in fr['cells'].items():
  obj=folder/label/'candidate.o';ci=inspect(obj)
  invariants={key:ci[key]==bi[key] for key in ['records','allocated','nobits','relocations']};assert all(invariants.values()),(label,invariants)
  names=d.b.sizes(str(base));total+=len(names)
  changed=sorted(n for n in names if d.b.body(str(base),n)!=d.b.body(str(obj),n));assert changed==sorted(cell.get('changed_bodies',[]))
  report[label]={'body_denominator':len(names),'invariants':invariants,'changed':changed,'target_grade':cell['verdicts'][TARGET],'target_bytes':d.b.sizes(str(obj))[TARGET],'gains':cell.get('gains',[]),'losses':cell.get('losses',[])}
 assert report['common-loop']['changed']==[TARGET]
 report['body_denominator']=total
 (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Sine entry audit:2cells,%demitted body comparisons,1rawbaseline reproduced; metadata/data/BSS/relocations fixed. Known entry-body change detected1/1. No width matrix executed.'%total)
if __name__=='__main__':main()
