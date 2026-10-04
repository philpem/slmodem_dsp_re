#!/usr/bin/env python3
"""Static CALL/tail relocation census for immutable C++ DSP/V90 period TUs."""
import argparse,collections,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:]=[p for p in sys.path if Path(p or '.').resolve()!=ROOT/'tools']
import dis
sys.path.insert(0,str(ROOT/'tools/toolchain'))
import byteident as b
import jumptable
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def calls(path, diagnostics=None):
 controls=[];alias_edges=0;accepted_edges=0
 with path.open('rb') as f:
  elf=ELFFile(f);tab=elf.get_section_by_name('.symtab');owners=collections.defaultdict(list);records={}
  for sym in tab.iter_symbols():
   if sym['st_info']['type']=='STT_FUNC' and isinstance(sym['st_shndx'],int) and sym['st_size']:
    owners[sym['st_shndx']].append(sym)
    records[sym.name]={'size':sym['st_size'],'calls':collections.Counter(),'tails':collections.Counter(),'unproved_control_relocations':[]}
  for relsec in elf.iter_sections():
   if not isinstance(relsec,RelocationSection):continue
   sid=relsec['sh_info'];section=elf.get_section(sid)
   if not section['sh_flags']&4:continue
   data=section.data();syms=elf.get_section(relsec['sh_link'])
   for rel in relsec.iter_relocations():
    if rel['r_info_type']!=2:continue
    off=rel['r_offset'];own=[s for s in owners[sid] if s['st_value']<=off<s['st_value']+s['st_size']]
    target=syms.get_symbol(rel['r_info_sym']);addend=int.from_bytes(data[off:off+4],'little',signed=True)
    entry={'section':section.name,'offset':off,'owners':[x.name for x in own],'target':target.name,'addend':addend}
    reason=None
    if not own:reason='no sized function owner'
    elif len({(x['st_shndx'],x['st_value'],x['st_size']) for x in own})!=1:reason='overlapping unequal function ranges'
    elif off<1 or data[off-1] not in (0xe8,0xe9):reason='not direct E8/E9 relocation candidate'
    else:
     owner=own[0]
     decoded={row[0]:row for row in jumptable.instructions(str(path),section.name,owner['st_value'],owner['st_value']+owner['st_size'])}
     row=decoded.get(off-1)
     if not row or len(row[1])!=5 or row[2] not in ('call','jmp'):reason='not decoded direct rel32 boundary'
     elif not(target.name and target['st_info']['type'] in ('STT_FUNC','STT_NOTYPE') and addend==-4):reason='unproved named target/type/addend'
    if reason:
     entry['reason']=reason;controls.append(entry)
     for owner in own:records[owner.name]['unproved_control_relocations'].append(dict(entry,offset=off-owner['st_value']))
     continue
    accepted_edges+=1
    if len(own)>1:alias_edges+=1
    for owner in own:records[owner.name]['calls' if data[off-1]==0xe8 else 'tails'][target.name]+=1
 if diagnostics is not None:
  diagnostics.update({'accepted_physical_direct_edges':accepted_edges,'equal_range_alias_physical_edges':alias_edges,
   'unsupported_pc32_controls':controls,'unsupported_pc32_count':len(controls)})
 return records
def owner_controls():
 source=ROOT/'build/production-before/src_pump_v90_V90PreFilter.cpp.o'
 names=['_ZN12V90PreFilterC%dE23__tHardwareCodecTypes__P13V90Phase2InfoP13V90Parameters'%x for x in (1,2)]
 data=source.read_bytes()
 with source.open('rb') as stream:
  elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab')
  rows={sym.name:(i,sym) for i,sym in enumerate(tab.iter_symbols()) if sym.name in names}
  i,second=rows[names[1]];first=rows[names[0]][1]
  assert first['st_shndx']==second['st_shndx'] and first['st_size']==second['st_size'] and first['st_value']!=second['st_value']
  offset=tab['sh_offset']+i*tab['sh_entsize']
 directory=ROOT/'build/gcc3-alignment-callee-inventory-controls';directory.mkdir(exist_ok=True)
 reports={}
 baseline=calls(source)
 for label,delta in [('equal-range-alias',0),('unequal-overlap',1)]:
  altered=bytearray(data);altered[offset+4:offset+8]=(first['st_value']+delta).to_bytes(4,'little')
  path=directory/(label+'.o');path.write_bytes(altered)
  diagnostic={};records=calls(path,diagnostic)
  if delta==0:
   assert diagnostic['equal_range_alias_physical_edges']>0
   assert records[names[0]]['calls']==records[names[1]]['calls']==baseline[names[0]]['calls']
   assert any(x['reason']=='no sized function owner' for x in diagnostic['unsupported_pc32_controls'])
  else:
   assert any(x['reason']=='overlapping unequal function ranges' for x in diagnostic['unsupported_pc32_controls'])
  reports[label]=diagnostic
 result={'source_sha256':hashlib.sha256(data).hexdigest(),'controls':reports,'passed':True}
 (directory/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 return result

def controls():
 a=collections.Counter({'nanf':1});z=collections.Counter()
 assert dict(a-z)=={'nanf':1}
 assert not(a-a)
 assert dict(collections.Counter({'sqrt':2})-collections.Counter({'sqrt':1}))=={'sqrt':1}
 return {'positive':2,'negative':1,'passed':True}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--all-tus',action='store_true',help='scan all300 immutable source objects');p.add_argument('--objects',type=Path,default=ROOT/'build/production-before');p.add_argument('--output',type=Path,default=ROOT/'build/gcc3-alignment-callee-inventory.json');args=p.parse_args()
 search=['rg','--files','src','-g','*.c','-g','*.cpp','-g','*.S'] if args.all_tus else ['rg','--files','src/dsp','src/pump/v90','-g','*.cpp']
 sources=subprocess.check_output(search,cwd=ROOT,text=True).splitlines();objects=[]
 if args.all_tus and args.output==ROOT/'build/gcc3-alignment-callee-inventory.json':args.output=ROOT/'build/gcc3-alignment-callee-inventory-all.json'
 for source in sorted(sources):
  obj=args.objects/(source.replace('/','_')+'.o')
  if args.all_tus and not obj.is_file():continue
  assert obj.is_file(),obj;objects.append((source,obj))
 if args.all_tus:
  assert len(objects)==300
  assert {o.name for _,o in objects}=={o.name for o in args.objects.glob('*.o')}
 blob=ROOT/'ref/slmodemd/dsplibs.o';original_diagnostics={};original=calls(blob,original_diagnostics);object_diagnostics={};rows=[];common=0;exact=0;library_gaps=collections.Counter();library_missing=collections.Counter()
 # These are language/runtime/library entrypoints, not reconstructed helpers.
 library=set(('nan','nanf','nanl','memcpy','memmove','memset','malloc','calloc','free','realloc','sqrt','sqrtf','sqrtl','pow','powf','powl','sin','sinf','sinl','cos','cosf','cosl','tan','tanf','atan','atanf','atan2','atan2f','acos','acosf','asin','asinf','exp','expf','log','logf','log10','log10f','fmod','fmodf','fabs','fabsf','ceil','ceilf','floor','floorf','round','roundf','ldexp','ldexpf','strcpy','strlen'))
 for source,obj in objects:
  diagnostics={};ours=calls(obj,diagnostics);object_diagnostics[str(obj)]=diagnostics
  for name,c in ours.items():
   if name not in original:continue
   common+=1;old=original[name];gap=c['calls']-old['calls'];missing=old['calls']-c['calls'];tailgap=c['tails']-old['tails'];tailmissing=old['tails']-c['tails']
   lib={k:n for k,n in gap.items() if k in library};libmissing={k:n for k,n in missing.items() if k in library}
   ab,ar=b.body(str(blob),name);bb,br=b.body(str(obj),name);grade=b.verdict(ab,ar,bb,br);exact+=grade[0]=='EXACT'
   if not(gap or missing or tailgap or tailmissing):continue
   excluded=('V90AutoDigitalImpDetector.cpp' in source or 'V90ConstellationDesigner.cpp' in source or 'V90Demodulator.cpp' in source)
   library_gaps.update(lib);library_missing.update(libmissing)
   rows.append({'symbol':name,'source':source,'object':str(obj),'blob_size':old['size'],'ours_size':c['size'],'grade':grade,
    'calls_blob':dict(old['calls']),'calls_ours':dict(c['calls']),'added_calls':dict(gap),'missing_calls':dict(missing),'added_call_or_tail_targets':dict((c['calls']+c['tails'])-(old['calls']+old['tails'])),
    'missing_call_or_tail_targets':dict((old['calls']+old['tails'])-(c['calls']+c['tails'])),
    'added_library_calls':lib,'missing_library_calls':libmissing,
    'added_library_call_or_tail_targets':{k:n for k,n in ((c['calls']+c['tails'])-(old['calls']+old['tails'])).items() if k in library},
    'missing_library_call_or_tail_targets':{k:n for k,n in ((old['calls']+old['tails'])-(c['calls']+c['tails'])).items() if k in library},'tail_calls_blob':dict(old['tails']),'tail_calls_ours':dict(c['tails']),
    'unproved_controls_blob':old['unproved_control_relocations'],'unproved_controls_ours':c['unproved_control_relocations'],
    'closed_or_other_session_excluded':excluded})
 known_nan=[r for r in rows if r['added_library_calls'].get('nanf')]
 assert len(known_nan)==2 and all(r['closed_or_other_session_excluded'] for r in known_nan)
 assert exact>0
 real_controls={'closed_nan_positive_rows':len(known_nan),'exact_negative_copies':exact,'passed':True}
 result={'owner_controls':owner_controls(),'direct_control_diagnostics':{'blob':original_diagnostics,'objects':object_diagnostics},'real_controls':real_controls,'call_tail_only_gap_copies':sum(not(r['added_call_or_tail_targets'] or r['missing_call_or_tail_targets']) for r in rows),'revision':'2001434c','scope':'all300' if args.all_tus else 'cpp-dsp-v90','translation_units':len(objects),'cpp_tus':sum(source.endswith('.cpp') for source,_ in objects),'emitted_common_copies':common,'exact_emitted_copies':exact,
  'call_boundary_gap_copies':len(rows),'extra_library_call_occurrences':dict(library_gaps),'missing_library_call_occurrences':dict(library_missing),'controls':controls(),
  'ranked_library_candidates':[r['symbol'] for r in sorted(rows,key=lambda r:r['blob_size']) if r['added_library_calls'] and not r['closed_or_other_session_excluded']],
  'ranked_missing_library_candidates':[{'symbol':r['symbol'],'source':r['source'],'missing_library_calls':r['missing_library_calls'],'missing_library_call_or_tail_targets':r['missing_library_call_or_tail_targets']} for r in sorted(rows,key=lambda r:r['blob_size']) if r['missing_library_calls'] and not r['closed_or_other_session_excluded']],
  'rows':rows,'object_hashes':{str(o):hashlib.sha256(o.read_bytes()).hexdigest() for _,o in objects},
  'blob_sha256':hashlib.sha256(blob.read_bytes()).hexdigest(),'build_config':(args.objects/'.build-config').read_text()}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps({k:result[k] for k in ('translation_units','cpp_tus','emitted_common_copies','exact_emitted_copies','call_boundary_gap_copies','extra_library_call_occurrences','missing_library_call_occurrences','ranked_library_candidates','ranked_missing_library_candidates','controls')}))
def prefilter_variants(path, source):
 cells={}
 for case2 in (0,1):
  for default in (0,1):
   text=source
   for enabled,bank in ((case2,2),(default,1)):
    if enabled:
     old='setCoefficients(bank%d((int)gain), 20);\n\t\tbreak;'%bank
     assert text.count(old)==1
     text=text.replace(old,old.replace('break;','return;'))
   cells['baseline' if not(case2 or default) else 'return-case2-%d-default-%d'%(case2,default)]=text
 return cells

def reproduce_prefilter():
 sys.path.insert(0,str(ROOT/'tools'))
 import playbook_small_patterns as d
 d.REV='2001434c';d.OUT_NAME='gcc3-alignment-callee-inventory-prefilter'
 d.SOURCE_PATHS=('src/pump/v90/V90PreFilter.cpp',);d.variants=prefilter_variants
 sys.argv.remove('--prefilter-reproduce');d.main()

def audit_prefilter():
 sys.path.insert(0,str(ROOT/'tools'))
 from gcc3_value_carriers_audit import inspect
 folder=ROOT/'build/gcc3-alignment-callee-inventory-prefilter'
 cells=json.loads((folder/'results.json').read_text())['families']['V90PreFilter']['cells']
 target='_ZN12V90PreFilter9setFilterE17PreFilterCoefTypej'
 basepath=folder/'V90PreFilter/baseline/candidate.o';base=inspect(basepath);reports={}
 assert len(cells)==4 and cells['baseline']['baseline_reproduced']
 for label,cell in cells.items():
  path=folder/'V90PreFilter'/label/'candidate.o';q=inspect(path)
  for key in ('records','allocated','nobits','relocations'):
   assert q[key]==base[key],(label,key)
  bystanders={}
  for name in cell['functions']:
   if name==target:continue
   a,ar=b.body(str(basepath),name);bb,br=b.body(str(path),name)
   assert a==bb,(label,name,'raw bystander body')
   assert ar==br,(label,name,'canonical relocation map')
   bystanders[name]={'raw_body_identical':True,'canonical_relocations_identical':True,'self_verdict':b.verdict(a,ar,bb,br)}
  assert not cell.get('gains',[]) and not cell.get('losses',[])
  report=calls(path)[target];report={k:dict(v) if isinstance(v,collections.Counter) else v for k,v in report.items()}
  ordinary=1 if label.endswith('default-1') else 2
  assert report['calls'].get('_ZN8FloatFIR15setCoefficientsEPfj')==ordinary
  assert report['tails'].get('_ZN8FloatFIR15setCoefficientsEPfj')==3-ordinary
  report.update({'bystanders':bystanders,'target_verdict':cell['verdicts'][target],
   'all_allocated_nontext_and_relocations_identical':True,'metadata_imports_identical':True,
   'raw_changed_bodies':cell.get('changed_bodies',[])})
  reports[label]=report
 stage_counts={}
 for stage in ('01.rtl','02.sibling'):
  stage_counts[stage]={}
  for label in ('baseline','return-case2-0-default-1'):
   text=(folder/'V90PreFilter'/label/('V90PreFilter.cpp.'+stage)).read_text()
   chunk=[x for x in text.split(';; Function ') if 'setFilter(PreFilterCoefType' in x.split('\n')[0]][0]
   stage_counts[stage][label]={'ordinary_call_nodes':chunk.count('(call_insn '),'sibling_call_nodes':chunk.count('(call_insn/j ')}
 assert stage_counts['02.sibling']['baseline']=={'ordinary_call_nodes':2,'sibling_call_nodes':1}
 assert stage_counts['02.sibling']['return-case2-0-default-1']=={'ordinary_call_nodes':1,'sibling_call_nodes':2}
 (folder/'stage-call-topology.json').write_text(json.dumps(stage_counts,indent=2)+'\n')
 (folder/'complete-tu-audit.json').write_text(json.dumps({'full_tus':4,'reports':reports},indent=2)+'\n')
 print('4/4 full-TU audits; 15 raw-identical bystanders/cell; metadata/data/imports/nontext relocations unchanged; zero gains/losses')

if __name__=='__main__':
 if '--prefilter-reproduce' in sys.argv:reproduce_prefilter()
 elif '--prefilter-audit' in sys.argv:audit_prefilter()
 else:main()
