#!/usr/bin/env python3
"""Whole-TU live validation for the quality use-boundary controls."""
import hashlib,json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
import gcc3_reload_trace as rtl

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 root=d.ROOT/'build/fax-quality-use';saved=json.loads((root/'results.json').read_text());rows=[]
 for family,data in saved['families'].items():
  n=family[1:3];target='QualityDetectV'+n
  basepath=root/family/'baseline/candidate.o';base=inspect(basepath);names=sorted(d.b.sizes(basepath))
  assert sha(basepath)==data['retained_hash'] and data['cells']['baseline']['baseline_reproduced']
  basebodies={name:d.b.body(basepath,name) for name in names}
  for label,cell in data['cells'].items():
   folder=root/family/label;obj=folder/'candidate.o'
   assert cell['compile_exit']==0 and sha(obj)==cell['object_hash']
   assert sha(folder/(family+'.c'))==cell['source_hash']
   observed=inspect(obj)
   for key in ['records','allocated','nobits','relocations']:assert observed[key]==base[key],(family,label,key)
   assert sorted(d.b.sizes(obj))==names
   live={name:list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(obj,name))) for name in names}
   assert live==cell['verdicts']
   changed=sorted(name for name in names if d.b.body(obj,name)!=basebodies[name])
   assert changed==cell.get('changed_bodies',[])
   assert set(changed)<=set([target,'EpochDetectV29'])
   gains=[name for name in names if live[name][0]=='EXACT' and data['cells']['baseline']['verdicts'][name][0]!='EXACT']
   losses=[name for name in names if live[name][0]!='EXACT' and data['cells']['baseline']['verdicts'][name][0]=='EXACT']
   assert gains==cell.get('gains',[]) and losses==cell.get('losses',[]) and not gains
   expected=['EpochDetectV29'] if n=='29' and label in ['tail-1-member-0-narrow-0','tail-1-member-0-narrow-1'] else []
   assert losses==expected
   rows.append({'family':family,'label':label,'body_verdicts':len(names),'target_size':len(d.b.body(obj,target)[0]),'changed_bodies':changed,'gains':gains,'losses':losses,'data_metadata_nontext_relocations_identical':True,'live_hash_verdict_validation':True})
 trace=[]
 family=root/'V29r_int'
 for stage in ['01.rtl','06.cse','20.combine','24.lreg','25.greg','26.postreload','27.flow2','28.peephole2','29.ce3','30.rnreg']:
  streams=[]
  for label in ['baseline','tail-1-member-0-narrow-0']:
   chunk=rtl.function((family/label/('V29r_int.c.'+stage)).read_text(),'EpochDetectV29')
   if stage=='25.greg':
    marker=';; Start of basic block 0,';assert chunk.count(marker)==1;chunk=chunk[chunk.index(marker):]
   streams.append(rtl.instructions(chunk))
  different=[uid for uid in sorted(set(streams[0])|set(streams[1])) if streams[0].get(uid)!=streams[1].get(uid)]
  expected=[38,39] if stage in ['28.peephole2','29.ce3'] else [11,38,39] if stage=='30.rnreg' else []
  assert different==expected,(stage,different)
  trace.append({'stage':stage,'different_instruction_uids':different,'patterns':{str(uid):[streams[0].get(uid),streams[1].get(uid)] for uid in different}})
 report={'epoch_bystander_stage_trace':trace,'cells':len(rows),'live_body_verdicts':sum(row['body_verdicts'] for row in rows),'gains':0,'losing_cells':sum(bool(row['losses']) for row in rows),'distinct_loss_symbols':['EpochDetectV29'],'rows':rows}
 (d.ROOT/'build/fax-quality-use-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k not in ['rows','epoch_bystander_stage_trace']}))
if __name__=='__main__':main()
