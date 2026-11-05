#!/usr/bin/env python3
"""Compare bounded local-stack effect graphs; diagnostics never prove equivalence.

NATIVE_EFFECT_DEPENDENCY_TOOLS selects an explicit owner-helper directory.
"""
import argparse,json,pathlib,tempfile
if __package__:
    from . import effect_graph as eg, stack_graph as sg
else:
    import effect_graph as eg, stack_graph as sg

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',required=True,type=pathlib.Path);p.add_argument('--candidate',required=True,type=pathlib.Path);p.add_argument('--symbol',required=True,action='append');p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
 eg.need(not a.output.exists(),'output already exists: '+str(a.output))
 inputs={};paths=[];functions=[]
 with tempfile.TemporaryDirectory(prefix='stack-effect-inputs-') as td:
  for i,path in enumerate((a.native,a.candidate)):
   data=path.read_bytes();snapshot=pathlib.Path(td)/('%d.o'%i);snapshot.write_bytes(data);snapshot.chmod(0o400);eg.validate_object(snapshot);paths.append(snapshot);inputs[str(path.resolve())]={'sha256':eg.digest(data),'size':len(data)}
  for name in sorted(set(a.symbol)):
   x,y=(sg.extract(p,name) for p in paths);functions.append({'symbol':name,'native':eg.compact(x),'candidate':eg.compact(y),'comparison':sg.compare(x,y)})
 files=list(eg.ot.DEPENDENCIES)+[pathlib.Path(eg.__file__),pathlib.Path(sg.__file__),pathlib.Path(__file__),eg.HERE/'bootstrap.py']
 result={'schema':'native-stack-effect-diagnostic-v1','semantic_proof':False,'inputs':inputs,'analysis_sha256':{str(p.resolve()):eg.digest(p.read_bytes()) for p in files},'helper_inputs':eg.CONTEXT.manifest(),'selected_functions':len(functions),'functions':functions}
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'selected':len(functions),'refused':sum(f['comparison']['status']=='refused' for f in functions),'timing_or_predicate_nominations':sum(len(f['comparison']['findings']) for f in functions),'unpaired_anchors':sum(len(f['comparison'].get('unpaired',[])) for f in functions),'semantic_proof':False}))
if __name__=='__main__':main()
