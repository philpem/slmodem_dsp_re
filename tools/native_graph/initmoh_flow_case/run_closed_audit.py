#!/usr/bin/env python3
"""Explicit, optional runner for this one-function audit; installs nothing."""
from pathlib import Path
import argparse,hashlib,importlib,importlib.metadata,json,subprocess,sys

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--native',required=True,type=Path)
    ap.add_argument('--candidate',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args();source=Path(__file__).resolve().parent;out=args.output.resolve()
    missing=[];versions={}
    for module,package in [('z3','z3-solver'),('capstone','capstone'),('elftools','pyelftools')]:
        try:importlib.import_module(module);versions[package]=importlib.metadata.version(package)
        except importlib.metadata.PackageNotFoundError:missing.append(package+' (module imports, distribution metadata unavailable)')
        except ImportError:missing.append(package+' (module unavailable)')
    if missing:ap.error('required optional dependencies missing: '+', '.join(missing)+'; install them in your chosen Python environment before running this tool')
    if out.exists():ap.error('output directory already exists; choose a fresh directory to preserve prior evidence')
    inputs={}
    blobs={'native.o':args.native.resolve(),'candidate.o':args.candidate.resolve()}
    scripts={name:source/name for name in ['audit.py','controls.py','verify_domains.py','format_report.py']}
    copied={}
    for name,path in {**blobs,**scripts}.items():
        data=path.read_bytes();copied[name]=data;inputs[name]={'path':str(path),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    out.mkdir(parents=True)
    for name,data in copied.items():(out/name).write_bytes(data)
    commands=[]
    for name,command in [('audit',[sys.executable,str(out/'audit.py')]),('path-feasibility',[sys.executable,str(out/'verify_domains.py')]),('controls-normal',[sys.executable,str(out/'controls.py')]),('controls-optimized',[sys.executable,'-O',str(out/'controls.py')]),('format',[sys.executable,str(out/'format_report.py')])]:
        with (out/(name+'.log')).open('w') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT)
        commands.append({'name':name,'argv':command,'returncode':result.returncode})
        (out/'runner-manifest.json').write_text(json.dumps({'inputs':inputs,'python':sys.version,'versions':versions,'commands':commands},indent=2)+'\n')
        print(f'{name}: exit {result.returncode}',flush=True)
        if result.returncode:return result.returncode
    return 0

if __name__=='__main__':sys.exit(main())
