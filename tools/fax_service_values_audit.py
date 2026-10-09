#!/usr/bin/env python3
"""Audit every live service cell, explicitly recording pools and jump tables."""
import hashlib,json,collections,subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from elftools.elf.elffile import ELFFile

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def pool(path):
 with path.open('rb') as stream:
  section=ELFFile(stream).get_section_by_name('.rodata.str1.1')
  assert section['sh_flags']==0x32 and section['sh_addralign']==1 and section['sh_entsize']==1
  raw=section.data();assert raw.endswith(b'\0')
  chunks=[];offset=0
  for text in raw.split(b'\0')[:-1]:
   chunks.append([offset,text.hex()]);offset+=len(text)+1
  return chunks

def main():
 reports=[]
 for package in ['fax-service-values','fax-service-factoring']:
  root=d.ROOT/'build'/package;data=json.loads((root/'results.json').read_text())['families']['fax']
  basepath=root/'fax/baseline/candidate.o';base=inspect(basepath);names=sorted(d.b.sizes(basepath));basepool=pool(basepath)
  assert len(names)==4
  assert sha(basepath)==data['retained_hash'] and data['cells']['baseline']['baseline_reproduced']
  basebodies={name:d.b.body(basepath,name) for name in names}
  for label,cell in data['cells'].items():
   folder=root/'fax'/label;obj=folder/'candidate.o'
   assert cell['compile_exit']==0 and sha(obj)==cell['object_hash'] and sha(folder/'fax.c')==cell['source_hash']
   value=inspect(obj)
   assert value['records']==base['records'] and value['nobits']==base['nobits']
   assert sorted(d.b.sizes(obj))==names
   six=package=='fax-service-factoring' and label.endswith('sixcase-1')
   changes=[]
   assert set(value['allocated'])==set(base['allocated'])
   for name in base['allocated']:
    if value['allocated'][name]==base['allocated'][name]:continue
    if name=='.rodata.str1.1':
     currentpool=pool(obj)
     assert collections.Counter(text for offset,text in currentpool)==collections.Counter(text for offset,text in basepool)
     assert len(bytes.fromhex(value['allocated'][name]))==len(bytes.fromhex(base['allocated'][name]))==375
     changes.append({'section':name,'kind':'same exact NUL-terminated literal multiset; offsets reordered','before_pool':basepool,'after_pool':currentpool})
    elif name=='.rodata':
     assert six and bytes.fromhex(value['allocated'][name])==b'\0'*68 and bytes.fromhex(base['allocated'][name])==b'\0'*64
     changes.append({'section':name,'kind':'one additional relocation-covered command jump-table entry','before_size':64,'after_size':68})
    else:raise AssertionError((package,label,name,'unreviewed allocated bytes'))
   rel=value['relocations'];counts=collections.Counter()
   for section,offset,kind,target in rel:
    assert section=='.rodata' and kind=='R_386_32' and target[0]=='audited-code-destination'
    counts[target[1]]+=1
   assert counts=={'FAX_class1_command':6 if six else 5,'FAX_process':11},(package,label,counts)
   assert [row[1] for row in rel]==list(range(0,68 if six else 64,4))
   live={name:list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(obj,name))) for name in names}
   assert live==cell['verdicts']
   changed=sorted(name for name in names if d.b.body(obj,name)!=basebodies[name])
   assert changed==cell.get('changed_bodies',[]) and 'FAX_delete' not in changed
   if not six:assert 'FAX_process' not in changed
   gains=[name for name in names if live[name][0]=='EXACT' and data['cells']['baseline']['verdicts'][name][0]!='EXACT']
   losses=[name for name in names if live[name][0]!='EXACT' and data['cells']['baseline']['verdicts'][name][0]=='EXACT']
   assert gains==cell.get('gains',[]) and losses==cell.get('losses',[]) and not gains and not losses
   # Source-identical bystanders can still change; preserve that inventory.
   production=subprocess.check_output(['git','show','a95a6c65:'+data['source_path']],cwd=d.ROOT,text=True)
   candidate=(folder/'fax.c').read_text()
   unchanged_source=[name for name in names if d.function(production,name)[2]==d.function(candidate,name)[2]]
   reports.append({'package':package,'label':label,'body_verdicts':4,'changed_bodies':changed,'source_unchanged_changed_bodies':sorted(set(changed)&set(unchanged_source)),'allocated_changes':changes,'nontext_jump_tables':{name:count for name,count in counts.items()},'nontext_relocations':rel,'gains':gains,'losses':losses,'metadata_bss_identical':True,'live_hash_verdict_validation':True})
 report={'cells':len(reports),'live_body_verdicts':4*len(reports),'gains':0,'losses':0,'reports':reports}
 (d.ROOT/'build/fax-service-values-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='reports'}))
if __name__=='__main__':main()
