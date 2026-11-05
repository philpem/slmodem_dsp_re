#!/usr/bin/env python3
"""Snapshot all inputs, select every inexact defining copy, bounded graph screen."""
import argparse,collections,json,pathlib,sys,time,shutil,subprocess
if __package__:
    from . import effect_graph as eg, stack_graph as sg
else:
    import effect_graph as eg, stack_graph as sg

def summary(rows):
 c=collections.Counter(); reasons=collections.Counter()
 for row in rows:
  for layer in ('owner','stack'):
   block=row[layer];cmp=block['comparison'];c[layer+'_'+cmp['status']]+=1
   if cmp['status']=='bounded':
    c[layer+'_copies_with_matched_anchors']+=cmp['matched_anchors']>0;c[layer+'_copies_with_compared_relations']+=cmp['relations_compared']>0;c[layer+'_bounded_without_matched_anchors']+=cmp['matched_anchors']==0;c[layer+'_matched_anchors']+=cmp['matched_anchors'];c[layer+'_relations_compared']+=cmp['relations_compared'];c[layer+'_cyclic_relations_excluded']+=cmp['cyclic_relations_excluded'];c[layer+'_ambiguous_anchors']+=cmp['ambiguous_anchor_count'];c[layer+'_unpaired_anchors']+=len(cmp['unpaired']);c[layer+'_timing_or_predicate_nominations']+=len(cmp['findings']);c[layer+'_copies_with_timing_nominations']+=bool(cmp['findings'])
   for side in ('native','candidate'):
    if block[side]['status']!='bounded':reasons[(layer,side,block[side].get('reason','unknown'))]+=1
 return {'counts':dict(c),'refusal_reasons':[{'layer':x[0],'side':x[1],'reason':x[2],'count':n} for x,n in reasons.most_common()]}

def bounded(a,b):
 if max(len(a.get('events',[])),len(b.get('events',[])))>180:return {'status':'comparison_bound','reason':'more than180 extracted events','matched_anchors':0,'findings':[]}
 return eg.compare(a,b)

def run(native,candidate_dir,output_dir):
 BASE=pathlib.Path(candidate_dir);OUT=pathlib.Path(output_dir);eg.need(not OUT.exists(),'output directory already exists: '+str(OUT));OUT.mkdir(parents=True)
 start=time.time();snap=OUT/'inputs';snap.mkdir(exist_ok=True);manifest={};candidate_snapshots=[]
 candidates=sorted(BASE.glob('*.o'));eg.need(bool(candidates),'no candidate object files')
 for index,path in enumerate([native]+candidates):
  data=path.read_bytes();dest=snap/'native.o' if index==0 else snap/'candidate'/path.name;dest.parent.mkdir(parents=True,exist_ok=True)
  if dest.exists():eg.need(dest.read_bytes()==data,'existing snapshot differs')
  else:dest.write_bytes(data);dest.chmod(0o400)
  eg.validate_object(dest)
  if index:candidate_snapshots.append(dest)
  manifest[str(path)]={'snapshot':str(dest),'sha256':eg.digest(data),'size':len(data)}
 (OUT/'input-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');
 if (BASE/'manifest.json').exists():shutil.copyfile(BASE/'manifest.json',OUT/'baseline-manifest.json')
 native=snap/'native.o';native_functions=eg.bi.function_owners(str(native))[0];allrows=[];grades=collections.Counter();uniquegrades=collections.defaultdict(set);ambiguous=[]
 for obj in candidate_snapshots:
  functions=eg.bi.function_owners(str(obj))[0]
  for name,defs in sorted(functions.items()):
   if name not in native_functions:continue
   if len(defs)!=1 or len(native_functions[name])!=1:ambiguous.append({'object':obj.name,'symbol':name,'candidate_definitions':defs,'native_definitions':native_functions[name]});continue
   a,ar=eg.bi.body(str(native),name);b,br=eg.bi.body(str(obj),name);grade,delta=eg.bi.verdict(a,ar,b,br)
   if grade=='BYTES':
    try:
     if eg.bi.alpha_equal(eg.bi.insns(str(native),name),eg.bi.insns(str(obj),name)):grade='REGALLOC'
    except ValueError:pass
   grades[grade]+=1;uniquegrades[name].add(grade)
   if grade=='EXACT':continue
   row={'object':obj.name,'symbol':name,'grade':grade,'byte_delta':delta,'native_size':len(a) if a is not None else None,'candidate_size':len(b) if b is not None else None}
   for layer,func in (('owner',eg.extract),('stack',sg.extract)):
    pair=[]
    for path in (native,obj):
     try:r=func(path,name)
     except (ValueError,RuntimeError,KeyError,TypeError) as e:r={'status':'refused','reason':type(e).__name__+': '+str(e)}
     pair.append(r)
    x,y=pair;row[layer]={'native':eg.compact(x),'candidate':eg.compact(y),'comparison':bounded(x,y)}
   allrows.append(row)
  print('screened',obj.name,'selected',len(allrows),flush=True)
 eg.need(bool(uniquegrades),'no shared uniquely owned function symbols')
 result={'schema':'native-effect-corpus-prototype-v1','semantic_proof':False,'elapsed_seconds':time.time()-start,'objects':len(candidate_snapshots),'defining_copy_grade_counts':dict(grades),'unique_shared_symbols':len(uniquegrades),'unique_symbols_any_inexact':sum(v!={'EXACT'} for v in uniquegrades.values()),'inexact_defining_copies_selected':len(allrows),'ambiguous_definition_rows':ambiguous,'summary':summary(allrows),'functions':allrows,'tool_sha256':{p.name:eg.digest(p.read_bytes()) for p in [eg.HERE/'effect_graph.py',eg.HERE/'stack_graph.py',eg.HERE/'screen.py',eg.HERE/'bootstrap.py']},'inputs_manifest':str(OUT/'input-manifest.json'),'helper_inputs':eg.CONTEXT.manifest()}
 (OUT/'corpus.json').write_text(json.dumps(result,indent=2)+'\n')
 ranked=[]
 for r in allrows:
  n=sum(len(r[l]['comparison']['findings']) for l in ('owner','stack'));u=sum(len(r[l]['comparison'].get('unpaired',[])) for l in ('owner','stack'))
  if n or u:ranked.append({'symbol':r['symbol'],'object':r['object'],'grade':r['grade'],'timing_nominations':n,'unpaired_effect_anchors':u,'owner_status':r['owner']['comparison']['status'],'stack_status':r['stack']['comparison']['status']})
 ranked.sort(key=lambda r:(-r['timing_nominations'],-r['unpaired_effect_anchors'],r['symbol']));(OUT/'ranked.json').write_text(json.dumps(ranked,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ('functions','ambiguous_definition_rows')},indent=2))
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',required=True,type=pathlib.Path);parser.add_argument('--candidate-dir',required=True,type=pathlib.Path);parser.add_argument('--output-dir',required=True,type=pathlib.Path);args=parser.parse_args()
 run(args.native,args.candidate_dir,args.output_dir)
if __name__=='__main__':main()
