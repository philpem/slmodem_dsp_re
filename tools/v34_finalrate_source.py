#!/usr/bin/env python3
"""Retained-profile full-TU rate-source and carrier controls; no fuzzing/mutation."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import experiment_toolchain as tc
sys.path.insert(0,str(ROOT/'tools/toolchain'))
import byteident as b


def rewrite(function, addresses, inputs, width, dispatch):
    text = function
    if addresses == 'root':
        declarations = re.findall(r'^\t(short \*|const short \*\*)(\w+)\s*=\s*([^;]+);\n',text,re.M)
        assert len(declarations)==8,len(declarations)
        for typename, name, expression in declarations:
            pattern = r'^\t'+re.escape(typename+name)+r'\s*=\s*'+re.escape(expression)+r';\n'
            text, count = re.subn(pattern,'',text,flags=re.M)
            assert count==1,(name,count)
            text, count = re.subn(r'\*'+name+r'\b',lambda m:'*'+expression,text)
            assert count>0,name
    if inputs == 'fresh':
        declarations = re.findall(r'^\tunsigned short (a9de|a9e0|a9e2) = ([^;]+);\n',text,re.M)
        assert len(declarations)==3,len(declarations)
        for name, expression in declarations:
            text, count = re.subn(r'^\tunsigned short '+name+r' = '+re.escape(expression)+r';\n','',text,flags=re.M)
            assert count==1
            text,count = re.subn(r'\b'+name+r'\b',lambda m:'('+expression+')',text)
            assert count>0
    if width == 'short':
        assert text.count('\tint code;')==1
        text=text.replace('\tint code;','\tshort code;')
    if dispatch == 'chain':
        pattern = r'\tswitch \(code\) \{\n(.*?)\tdefault:\n\t\tbreak;\n\t\}'
        blocks=list(re.finditer(pattern,text,re.S)); assert len(blocks)==2,len(blocks)
        for block in reversed(blocks):
            arms=re.findall(r'\tcase (\d+):\n(.*?)(?=\tcase |\Z)',block[1],re.S)
            assert [int(n) for n,_ in arms]==[0,2,3,4,5]
            lines=[]
            for index,(number,body) in enumerate(arms):
                assert body.endswith('\t\tbreak;\n'),repr(body[-40:])
                lines.append(('\tif' if index==0 else '\t} else if')+' (code == '+number+') {\n'+body[:-len('\t\tbreak;\n')])
            lines.append('\t}')
            text=text[:block.start()]+''.join(lines)+text[block.end():]
    return text


def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('--carrier-study',action='store_true'); args=ap.parse_args()
    out=ROOT/('build/v34-finalrate-carrier' if args.carrier_study else 'build/v34-finalrate-source');out.mkdir(parents=True,exist_ok=True)
    revision='d67e4042'
    source=subprocess.check_output(['git','show',revision+':src/pump/v34/V34hshak.c'],cwd=ROOT,text=True)
    start=source.index('void\nsetfinalrate(');end=source.index('\n}\n',start)+2
    function=source[start:end]
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    variants=[('baseline',None)]+[('-'.join(cell),cell) for cell in itertools.product(('cached','root'),('cached','fresh'),('int','short'),('switch','chain'))]
    # Validate the complete source domain before any compilation or score.
    generated={name:source if cell is None else source[:start]+rewrite(function,*cell)+source[end:] for name,cell in variants}
    assert len(generated)==17
    assert len(set(generated.values()))==16 # baseline is one product cell
    if args.carrier_study:
        generated={'baseline':source}
        for width,carrier in itertools.product(('int','short'),('local','byte','word')):
            fn=rewrite(function,'root','fresh',width,'chain')
            if carrier!='local':
                old='\thigh = ((*(unsigned short *)(m + 0xa9de)) & 4) != 0;\n'
                assert fn.count(old)==1
                fn=fn.replace('\tint high;\n','').replace(old,'')
                value='(m[0xa9de] & 4)' if carrier=='byte' else '(*(unsigned short *)(m + 0xa9de) & 4)'
                fn,count=re.subn(r'\bhigh\b',lambda m:value,fn)
                assert count==4,count
            generated[width+'-'+carrier]=source[:start]+fn+source[end:]
        variants=[(name,None) for name in generated]
        assert len(generated)==7
        assert len(set(generated.values()))==7

    blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    prior=(out/'baseline/V34hshak.o' if (out/'baseline/V34hshak.o').exists() else ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o').read_bytes()
    results={'revision':revision,'config':config,'domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5936835816','cells':{}}
    if args.carrier_study:
        results['domain']='https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5936926118'
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    for name,cell in variants:
        directory=out/name;directory.mkdir(parents=True,exist_ok=True)
        text=generated[name];(directory/'V34hshak.c').write_text(text)
        target='/work/'+name
        command=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(target)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+['-da'],target+'/V34hshak.o',target+'/V34hshak.c')]
        entry={'command':command,'source_hash':hashlib.sha256(text.encode()).hexdigest()};results['cells'][name]=entry
        with (directory/'compile.log').open('w') as log:
            entry['compile_exit']=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        assert entry['compile_exit']==0,name
        obj=str(directory/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals']={line.split()[-1]:line.split()[-2] for line in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['function_symbols']=sorted(b.sizes(obj))
        entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        (directory/'setfinalrate.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=setfinalrate',obj],text=True))
        if name=='baseline':
            assert Path(obj).read_bytes()==prior,'raw unchanged baseline drift'
            entry['baseline_reproduced']=True
        else:
            base=results['cells']['baseline'];assert entry['globals']==base['globals']
            assert entry['function_symbols']==base['function_symbols']
            baseline=str(out/'baseline/V34hshak.o')
            entry['changed_bodies']=[sym for sym in entry['function_symbols'] if b.body(obj,sym)!=b.body(baseline,sym)]
            entry['exact_gains']=[sym for sym,v in entry['verdicts'].items() if v[0]=='EXACT' and base['verdicts'][sym][0]!='EXACT']
            entry['exact_losses']=[sym for sym,v in base['verdicts'].items() if v[0]=='EXACT' and entry['verdicts'][sym][0]!='EXACT']
        print(name,entry['verdicts']['setfinalrate'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
