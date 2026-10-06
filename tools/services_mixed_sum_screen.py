#!/usr/bin/env python3
"""Read-only small-service triage for two different memory-load widths feeding ADD."""
import argparse,json,re
from pathlib import Path
import playbook_small_patterns as d
import jumptable
from elftools.elf.elffile import ELFFile
REG=re.compile(r'%e(?:ax|bx|cx|dx|si|di|bp)$')
def symbols(path):
 with path.open('rb') as f:
  elf=ELFFile(f);tab=elf.get_section_by_name('.symtab')
  return {s.name:(elf.get_section(s['st_shndx']).name,s['st_value'],s['st_size']) for s in tab.iter_symbols() if s['st_info']['type']=='STT_FUNC' and isinstance(s['st_shndx'],int) and s['st_size']}
def graphs(rows):
 found=[]
 for k,(address,raw,mn,ops) in enumerate(rows):
  regs=ops.split(',')
  if mn!='add' or len(regs)!=2 or not all(REG.fullmatch(r) for r in regs):continue
  loads={}
  for prior in reversed(rows[max(0,k-6):k]):
   _,_,pmo,pop=prior
   if pmo.startswith(('j','call','ret')):break
   for r in regs:
    if r in loads:continue
    if not pop.endswith(','+r):continue
    # Only record direct heterogeneous memory reads, not arbitrary register renames.
    kind='word-zero' if pmo=='movzwl' and '(' in pop else 'full' if pmo=='mov' and '(' in pop else 'other-write'
    loads[r]=[kind,prior[0],pmo,pop]
  if set(loads)==set(regs) and {loads[r][0] for r in regs}=={'word-zero','full'}:
   found.append({'at':address,'add':ops,'inputs':[loads[r] for r in regs]})
 return found

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--baseline-json',type=Path,default=d.ROOT/'build/baseline-byteident.json')
 parser.add_argument('--objects',type=Path,default=d.ROOT/'build/production-before')
 args=parser.parse_args()
 ref=symbols(Path(d.b.BLOB));exact=set(json.loads(args.baseline_json.read_text())['exact_symbols'])
 ledger={'objects':0,'nonexact_small_functions':0,'graph_candidates':[]}
 for obj in sorted(args.objects.glob('*.o')):
  if not obj.name.startswith(('src_core_','src_service_','src_voice_')) or any(x in obj.name for x in ('dp_wrapper','FixedRC','RcFixed')):continue
  ledger['objects']+=1
  for name,(section,start,size) in symbols(obj).items():
   if name not in ref or name in exact or size>350:continue
   ledger['nonexact_small_functions']+=1;rsection,rstart,rsize=ref[name]
   original=graphs(jumptable.instructions(d.b.BLOB,rsection,rstart,rstart+rsize))
   if not original:continue
   own=graphs(jumptable.instructions(str(obj),section,start,start+size))
   ledger['graph_candidates'].append({'symbol':name,'object':obj.name,'verdict':d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(obj),name)),'original':original,'retained':own})
 assert any(x['symbol']=='voice_modem' for x in ledger['graph_candidates']),'known mixed sum detector failed'
 (d.ROOT/'build/services-mixed-sum-screen.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(ledger,indent=2))
if __name__=='__main__':main()
