#!/usr/bin/env python3
"""Audit GenericIIR source-boundary controls without treating size as fidelity."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect,function_chunk
SCALAR='_ZN10GenericIIRIfdE7processEf'
BLOCK='_ZN10GenericIIRIfdE7processEPKfPfj'
def main():
 root=d.ROOT/'build/batch-cpp-iir';package=json.loads((root/'results.json').read_text());fr=package['families']['FloatIIR'];froot=root/'FloatIIR';base=froot/'baseline/candidate.o';retained=froot/'retained.o';assert base.read_bytes()==retained.read_bytes()
 bi=inspect(base);report={};count=0
 for label,cell in fr['cells'].items():
  obj=froot/label/'candidate.o';ci=inspect(obj)
  fixed={key:ci[key]==bi[key] for key in ['records','nobits','relocations']};assert all(fixed.values()),(label,fixed)
  changes={name:[bi['allocated'].get(name),ci['allocated'].get(name)] for name in set(bi['allocated'])|set(ci['allocated']) if bi['allocated'].get(name)!=ci['allocated'].get(name)}
  # Member-acc removes a numeric-zero compare pool; scalar inlining may duplicate it.
  assert set(changes)<=set(['.rodata.cst8']),(label,changes)
  for before,after in changes.values():
   assert all(byte==0 for hexdata in (before,after) if hexdata is not None for byte in bytes.fromhex(hexdata))
  bodies={name:d.b.body(str(base),name)==d.b.body(str(obj),name) for name in d.b.sizes(str(base))};count+=len(bodies)
  changed=[name for name,equal in bodies.items() if not equal];assert sorted(changed)==sorted(cell.get('changed_bodies',[]))
  rtl=function_chunk(froot/label/'FloatIIR.cpp.01.rtl','::process(Sample)')
  report[label]={'body_denominator':len(bodies),'fixed_invariants':fixed,'zero_constant_pool_changes':changes,'changed_bodies':changed,'common_gains':cell.get('gains',[]),'common_losses':cell.get('losses',[]),'m_acc_initial_rtl_references':rtl.count('.m_acc+0'),'scalar':cell['verdicts'][SCALAR],'block':cell['verdicts'][BLOCK]}
 assert report['member-acc']['m_acc_initial_rtl_references']>report['baseline']['m_acc_initial_rtl_references'],'member-publication positive control failed'
 assert BLOCK in report['baseline-inline-scalar']['changed_bodies'],'inline-scalar positive control failed'
 report['body_denominator']=count
 (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print('GenericIIR audit: %d cells, %d emitted body comparisons, rawbaseline reproduced; metadata/BSS/nontext relocations fixed; all changed nontext bytes are numeric-zero compare pools. Member-publication and inline-scalar positive controls fired2/2.'%(len(fr['cells']),count))
if __name__=='__main__':main()
