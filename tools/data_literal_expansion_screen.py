#!/usr/bin/env python3
"""Read-only repeated-call graph screen; raw counts nominate, never adopt."""
import argparse,json,re,subprocess
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
SCOPE=('src/pump/v32/','src/pump/v22/','src/pump/v23/','src/pump/b103/','src/v8/','src/service/','src/call/','src/callprog/')
def calls(path,name):
 text=subprocess.run(['objdump','-dr','--disassemble='+name,str(path)],check=True,text=True,capture_output=True).stdout
 rows=text.splitlines();out=[]
 for i,line in enumerate(rows):
  m=re.search(r'\bcall\s+[^<]*<([^>]+)>',line)
  if not m:continue
  target=m[1]
  if i+1<len(rows):
   relocation=re.search(r'R_386_PC32\s+(\S+)',rows[i+1])
   if relocation:target=relocation[1]
  if '+' not in target:out.append(target)
 return Counter(out),text

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--baseline-json',type=Path,default=d.ROOT/'build/baseline-byteident.json')
 parser.add_argument('--objects',type=Path,default=d.ROOT/'build/production-before')
 args=parser.parse_args()
 exact=set(json.loads(args.baseline_json.read_text())['exact_symbols'])
 original=d.b.sizes(str(d.b.BLOB));report={'translation_units':0,'emitted_bodies':0,'eligible_nonexact_bodies':0,'eligible_source_files':[],'nominations':[]}
 for path in sorted(d.ROOT.joinpath('src').rglob('*.c')):
  rel=str(path.relative_to(d.ROOT))
  if not rel.startswith(SCOPE):continue
  obj=args.objects/(rel.replace('/','_')+'.o')
  if not obj.exists():continue
  report['translation_units']+=1;report['eligible_source_files'].append(rel)
  names=d.b.sizes(str(obj));report['emitted_bodies']+=len(names)
  for name in names:
   if name in exact or name not in original or original[name]>800:continue
   report['eligible_nonexact_bodies']+=1
   blob,_=calls(d.b.BLOB,name);ours,_=calls(obj,name)
   missing={callee:[count,ours[callee]] for callee,count in blob.items() if count>=2 and ours[callee]==1}
   if missing:report['nominations'].append({'source':rel,'function':name,'blob_size':original[name],'ours_size':names[name],'repeat_differences':missing,'blob_calls':dict(blob),'ours_calls':dict(ours)})
 # Known independently verified rolled-vs-literal winner: detector must fire.
 positive='_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams'
 pb,_=calls(d.b.BLOB,positive)
 pp,_=calls(args.objects/'src_pump_v90_V90ConstellationPower.cpp.o',positive)
 assert pb['__moddi3']==5 and pb['__divdi3']==6 and pp['__moddi3']==1 and pp['__divdi3']==2
 report['positive_reference']={'function':positive,'blob':dict(pb),'ours':dict(pp),'detected':True}
 out=d.ROOT/'build/data-literal-expansion-screen.json';out.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='eligible_source_files'},indent=2))
if __name__=='__main__':main()
