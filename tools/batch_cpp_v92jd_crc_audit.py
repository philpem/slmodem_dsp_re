#!/usr/bin/env python3
"""Audit the bounded V92 CRC ownership transfer, including losing bystanders."""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

def main():
 root=d.ROOT/'build/batch-cpp-v92jd-crc';folder=root/'V92Jd';fr=json.loads((root/'results.json').read_text())['families']['V92Jd'];base=folder/'baseline/candidate.o';assert base.read_bytes()==(folder/'retained.o').read_bytes();bi=inspect(base);report={};total=0
 for label,cell in fr['cells'].items():
  obj=folder/label/'candidate.o';ci=inspect(obj);fixed={key:ci[key]==bi[key] for key in ['records','allocated','nobits','relocations']};assert all(fixed.values()),(label,fixed)
  names=d.b.sizes(str(base));total+=len(names)
  changed=sorted(n for n in names if d.b.body(str(base),n)!=d.b.body(str(obj),n));assert changed==sorted(cell.get('changed_bodies',[]))
  report[label]={'emitted_body_denominator':len(names),'fixed_invariants':fixed,'changed_bodies':changed,'gains':cell.get('gains',[]),'losses':cell.get('losses',[]),'relevant_grades':{n:v for n,v in cell['verdicts'].items() if any(word in n for word in ['packJdData','packJdPhaseData','getJdBitVector','getJdPhaseBitVector'])}}
 assert len(report['direct-members-rolled']['losses'])==2
 assert not report['direct-members-expanded']['gains'] and not report['direct-members-expanded']['losses']
 assert report['direct-members-expanded']['changed_bodies']!=report['baseline']['changed_bodies']
 report['emitted_body_denominator']=total;(root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('V92 CRC audit:4cells,%demitted body comparisons,rawbaseline reproduced; metadata/data/BSS/nontext relocs fixed. Losing direct/rolled control detected2exact losses; expanded/direct restores them,0complete gains.'%total)
if __name__=='__main__':main()
