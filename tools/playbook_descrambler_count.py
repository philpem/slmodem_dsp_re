#!/usr/bin/env python3
"""Three header-overlay allocation-count controls over every period consumer."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
# Resolve stdlib dis before exposing the analysis tools directory.
ROOT = Path(__file__).resolve().parents[1]
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT/'tools']
import dis
sys.path[:] = _script_path
sys.path.insert(0, str(ROOT/'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(command, log):
    with log.open('w') as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True)


def payload(obj):
    result = {}
    for line in subprocess.check_output(['objdump', '-h', str(obj)], text=True).splitlines():
        parts = line.split()
        if len(parts)>1 and parts[0].isdigit() and parts[1].startswith(('.data', '.rodata', '.bss')):
            name = parts[1]
            result[name] = subprocess.check_output(['objdump', '-s', '-j', name, str(obj)], text=True).splitlines()[2:]
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--domain', required=True)
    args = ap.parse_args()
    out = ROOT/'build/playbook-descrambler-count'
    out.mkdir(exist_ok=True)
    header = ROOT/'include/dsplib/Scrambler.h'
    original = header.read_text()
    old = '\ttailLength = b;\n\tpLimit = (T *)sysdep_malloc((1 + b + c) * sizeof(T));'
    assert original.count(old)==1
    variants = {'baseline': original,
                'reassociated': original.replace(old, old.replace('(1 + b + c)', '(b + c + 1)')),
                'count-local': original.replace(old, '\ttailLength = b;\n\tunsigned int count = b + c + 1;\n\tpLimit = (T *)sysdep_malloc(count * sizeof(T));')}
    assert len(set(variants.values()))==3
    config = (ROOT/'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    cxx = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
    manifest = [line.split() for line in (ROOT/'build/tc_out/tc_manifest.txt').read_text().splitlines()]
    headers = {str(p.relative_to(ROOT)):digest(p.read_bytes()) for p in (ROOT/'include').rglob('*.h')}
    result = {'revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(), 'domain':args.domain, 'config':config, 'header_hashes':headers, 'manifest':manifest, 'cells':{}}
    prefix = tc.docker_prefix(image, ROOT, out, True)
    identity = 'export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH; g++ --version; g++ -dumpmachine; selected_as=$(g++ -print-prog-name=as); "$selected_as" --version'
    result['identity_command'] = prefix+['/bin/sh','-ec',identity]
    run(result['identity_command'],out/'identity.log')
    deps = out/'dependencies'; deps.mkdir(exist_ok=True)
    commands = []
    for obj, source in manifest:
        if Path(source).suffix not in ('.c','.cpp'): continue
        compiler = 'g++' if source.endswith('.cpp') else 'gcc'
        command = compiler+' -M '+shlex.join(tc.add_reproduce_bugs(flags+(cxx if compiler=='g++' else [])))+' -MT '+shlex.quote(source)+' '+shlex.quote('/src/'+source)+' > '+shlex.quote('/work/dependencies/'+obj+'.d')
        commands.append(command)
    shell = 'set -e; export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH;\n'+'\n'.join(commands)
    result['dependency_command']=prefix+['/bin/sh','-c',shell]
    run(result['dependency_command'],out/'dependency.log')
    consumers = [(obj,source) for obj,source in manifest if (deps/(obj+'.d')).exists() and '/src/include/dsplib/Scrambler.h' in (deps/(obj+'.d')).read_text()]
    assert consumers
    result['dependency_sources']=len(commands);result['consumers']=consumers
    print('dependency discovery',len(commands),'/',len(manifest),'sources;',len(consumers),'consumers',flush=True)
    blob_sizes=b.sizes(b.BLOB)
    for label,text in variants.items():
        cell=out/label; overlay=cell/'include/dsplib'; overlay.mkdir(parents=True,exist_ok=True)
        (overlay/'Scrambler.h').write_text(text)
        entries={};result['cells'][label]={'header_hash':digest(text.encode()),'objects':entries}
        for obj,source in consumers:
            assert digest(header.read_bytes())==headers['include/dsplib/Scrambler.h']
            target=cell/obj
            selected_flags=['-I/work/'+label+'/include']+flags+(cxx if source.endswith('.cpp') else [])
            shell=tc.compile_shell(tc.GENTOO_COMPILER_PATH,selected_flags,'/work/'+label+'/'+obj,'/src/'+source)
            if source.endswith('.cpp'):shell=shell.replace('exec gcc -c ','exec g++ -c ',1)
            command=prefix+['/bin/sh','-c',shell]
            run(command,cell/(obj+'.log'))
            entry={'command':command,'source_hash':digest((ROOT/source).read_bytes()),'object_hash':digest(target.read_bytes())};entries[obj]=entry
            base=ROOT/'build/tc_out'/obj
            assert b.sizes(str(target)).keys()==b.sizes(str(base)).keys()
            def globals_(p):return [line.split()[1:] for line in subprocess.check_output(['nm','-g','--defined-only',str(p)],text=True).splitlines()]
            assert globals_(target)==globals_(base)
            assert payload(target)==payload(base)
            entry['nontext_equal'] = True
            def records(p):
                rows = []
                for line in subprocess.check_output(['readelf','-Ws',str(p)],text=True).splitlines():
                    fields = line.split()
                    if len(fields)>=8 and fields[0].rstrip(':').isdigit(): rows.append(fields[1:])
                return rows
            entry['symbol_records'] = records(target)
            base_records = records(base)
            assert [r[2:] for r in entry['symbol_records']] == [r[2:] for r in base_records]
            entry['changed_bodies']=[n for n in b.sizes(str(base)) if b.body(str(base),n)!=b.body(str(target),n)]
            entry['verdicts']={n:b.verdict(*b.body(b.BLOB,n),*b.body(str(target),n)) for n in b.sizes(str(target)) if n in blob_sizes}
            if label=='baseline':assert target.read_bytes()==base.read_bytes(),obj
            if entry['changed_bodies']:print(label,obj,'changes',[(n,entry['verdicts'].get(n)) for n in entry['changed_bodies']],flush=True)
            (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
        print(label,len(entries),'full consumer objects completed',flush=True)
    assert all(digest((ROOT/path).read_bytes())==value for path,value in headers.items())
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
