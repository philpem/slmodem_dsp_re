#!/usr/bin/env python3
"""Read-only C++/service mechanism census, preserving every defining copy."""
import argparse,collections,hashlib,json,re,subprocess
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
DIAGNOSTICS={'dsplibs_debug_printf','edprintf'}
def inventory(path):
 result={};boundaries={};current_section=None
 for line in subprocess.check_output(['objdump','-d',str(path)],text=True).splitlines():
  m=re.match(r'Disassembly of section (.+):',line)
  if m:current_section=m.group(1);continue
  m=re.match(r'^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*([a-z]+)',line)
  if m:boundaries[(current_section,int(m.group(1),16))]=m.group(3)
 with path.open('rb') as stream:
  elf=ELFFile(stream);table=elf.get_section_by_name('.symtab')
  funcs=[s for s in table.iter_symbols() if s['st_info']['type']=='STT_FUNC' and isinstance(s['st_shndx'],int)]
  for s in funcs:result[s.name]={'size':s['st_size'],'diagnostic_sites':[]}
  for sec in elf.iter_sections():
   if not isinstance(sec,RelocationSection):continue
   code=elf.get_section(sec['sh_info']).data()
   for r in sec.iter_relocations():
    target=table.get_symbol(r['r_info_sym']).name
    if target not in DIAGNOSTICS:continue
    off=r['r_offset'];opcode=code[off-1] if off else None
    kind={0xe8:'call',0xe9:'tail_jump'}.get(opcode,'unsupported')
    actual_mnemonic=boundaries.get((elf.get_section(sec['sh_info']).name,off-1))
    if r['r_info_type']!=2 or actual_mnemonic not in ('call','jmp') or (kind=='call' and actual_mnemonic!='call') or (kind=='tail_jump' and actual_mnemonic!='jmp'):kind='unsupported'
    for s in funcs:
     if s['st_shndx']==sec['sh_info'] and s['st_value']<=off<s['st_value']+s['st_size']:
      result[s.name]['diagnostic_sites'].append({'kind':kind,'target':target,'offset':off-s['st_value']-1,'relocation_type':r['r_info_type']})
 return result
def kinds(record):return collections.Counter(s['kind'] for s in record['diagnostic_sites'])
def owned(name):
 if 'V90Demodulator' in name or 'Beepgen' in name or 'FDSP' in name or 'Fdsp' in name:return False
 return ((name.startswith(('src_pump_v90_','src_dsp_')) and name.endswith('.cpp.o')) or name.startswith('src_service_'))
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--baseline-dir',type=Path,default=d.ROOT.parent/'byteexact-batch100/build/tc_out');ap.add_argument('--out',type=Path,default=d.ROOT/'build/gcc3-mechanism-owner-screen.json');args=ap.parse_args()
 objects=sorted(args.baseline_dir.glob('*.o'));assert len(objects)==300,'immutable full baseline required'
 config=(args.baseline_dir/'.build-config').read_text();assert '-DDSPLIB_REPRODUCE_BUGS' in config
 ref=inventory(Path(d.b.BLOB));rows=[];tail_rows=[];eligible=0;poolobjects=0
 for obj in objects:
  if not owned(obj.name):continue
  poolobjects+=1
  for symbol,q in inventory(obj).items():
   if symbol not in ref:continue
   eligible+=1;p=ref[symbol];v=d.b.verdict(*d.b.body(d.b.BLOB,symbol),*d.b.body(str(obj),symbol))
   if v[0]=='EXACT':continue
   row={'object':obj.name,'symbol':symbol,'blob_size':p['size'],'ours_size':q['size'],'verdict':v,'blob_diagnostics':p['diagnostic_sites'],'ours_diagnostics':q['diagnostic_sites']};rows.append(row)
   if kinds(p)['tail_jump']>kinds(q)['tail_jump'] and kinds(q)['call']>kinds(p)['call']:tail_rows.append(row)
 # F11782 known original-terminal control: detector must fire before trusting misses.
 old=args.baseline_dir.parent/'production-before/src_pump_v90_V92Modem.cpp.o';new=args.baseline_dir/'src_pump_v90_V92Modem.cpp.o';assert old.exists()
 oi=inventory(old);ni=inventory(new);names=[s for s in oi if s.startswith('_ZN8V92ModemC')];assert len(names)==2
 controls=[]
 for s in names:
  a=kinds(oi[s]);z=kinds(ni[s]);r=kinds(ref[s]);assert a['tail_jump']==0 and z['tail_jump']==r['tail_jump']==1 and a['call']==z['call']+1
  controls.append({'symbol':s,'before':dict(a),'after':dict(z),'blob':dict(r)})
 report={'baseline_revision':'9f1199b57d3c91d1f26627eabd9eea496355798d','baseline_objects':len(objects),'pool_objects':poolobjects,'eligible_defining_copies':eligible,'nonexact_defining_copies':len(rows),'nonexact_names':len({r['symbol'] for r in rows}),'terminal_diagnostic_candidates':len(tail_rows),'config':config,'config_sha256':hashlib.sha256(config.encode()).hexdigest(),'known_terminal_controls':controls,'rows':rows,'terminal_rows':tail_rows,'limits':'Counts are triage only. Retained-input/callback/union source ownership requires paired disassembly and prior-domain review; no source inference from size or counts.'}
 args.out.write_text(json.dumps(report,indent=2)+'\n')
 print(len(objects),'baseline objects;',poolobjects,'owned objects;',eligible,'eligible copies;',len(rows),'nonexact copies;',len(tail_rows),'terminal diagnostic candidates;2/2 known controls fire')
 for r in tail_rows:print(r['object'],r['symbol'],r['verdict'],dict(kinds({'diagnostic_sites':r['blob_diagnostics']})),dict(kinds({'diagnostic_sites':r['ours_diagnostics']})))
if __name__=='__main__':main()
