#!/usr/bin/env python3
"""Trace actual Gentoo cc1 stack allocation with host GDB (diagnostic only).

First run recovery_call_create_stack.py to generate both preprocessed TUs.
Supply --compiler with the actual unstripped Gentoo gcc -print-prog-name=cc1
binary copied from the period container. The host runtime is diagnostic;
Docker full-TU objects and period differentials remain the authority.
No downloaded GCC source is needed: optimized debug arguments are read from
verified i386 ABI stack slots, not unreliable DWARF parameter aliases.
"""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SCRIPT='''set pagination off
set confirm off
break expand_decl
commands
silent
if current_function_decl != 0 && current_function_decl->decl.name != 0
set $fn = current_function_decl->decl.name->identifier.id.str
if $_streq($fn, "call_create")
printf "EXPAND decl="
if decl->decl.name != 0
printf "%s", decl->decl.name->identifier.id.str
end
printf " frame=%ld\\n", cfun->x_frame_offset
bt 3
end
end
continue
end
break assign_stack_local_1
commands
silent
set $owner = *(struct function **)($ebp+0x14)
if $owner->decl != 0 && $owner->decl->decl.name != 0
set $fn = $owner->decl->decl.name->identifier.id.str
if $_streq($fn, "call_create")
printf "ALLOC size=%ld align=%d mode=%d frame_before=%ld\\n", *(int *)($ebp+0xc), *(int *)($ebp+0x10), *(int *)($ebp+8), $owner->x_frame_offset
bt 4
end
end
continue
end
run
'''

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--compiler',type=Path,required=True)
    ap.add_argument('--run-dir',type=Path,default=ROOT/'build/recovery-call-create-stack')
    args=ap.parse_args(); compiler=args.compiler.resolve(); root=args.run_dir.resolve()
    out=root/'compiler-trace';out.mkdir(exist_ok=True)
    command=['gdb','-q','-batch',str(compiler),'-ex','info line expand_decl','-ex','disassemble assign_stack_local_1','-ex','disassemble expand_decl']
    debug=subprocess.check_output(command,text=True,stderr=subprocess.STDOUT)
    (out/'actual-disassembly.txt').write_text(debug)
    # These instructions establish parameter capture and the frame-size rule
    # for the actual binary before the runtime script relies on its ABI.
    assert '0x14(%ebp),%esi' in debug and '0xc(%ebp),%edi' in debug
    assert 'sub    %edi,0xb8(%esi)' in debug
    script=out/'trace.gdb';script.write_text(SCRIPT)
    result={'scope':'actual compiler, host diagnostic runtime; original RTL unavailable',
            'compiler_sha256':sha(compiler),'debug_command':command,'cells':{}}
    cells=json.loads((root/'results.json').read_text())['families']['call']['cells']
    for label,cell in cells.items():
        tokens=shlex.split(cell['command'][-1]); start=tokens.index('gcc')+2
        flags=[t for t in tokens[start:tokens.index('-o')] if t.startswith(('-O','-f','-m'))]
        preprocessed=root/'call'/label/'call.i'
        cmd=['gdb','-q','-batch','-x',str(script),'--args',str(compiler),'-quiet']+flags+[str(preprocessed),'-o',str(out/(label+'.s'))]
        run=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/(label+'.log')).write_text(run.stdout)
        assert run.returncode==0 and 'exited normally' in run.stdout, (label,run.stdout[-1000:])
        expected = ('EXPAND decl=cfg frame=0','EXPAND decl=dialstr frame=-16') if label=='baseline' else ('EXPAND decl=dialstr frame=0','EXPAND decl=cfg frame=-64')
        assert all(s in run.stdout for s in expected),label
        assert run.stdout.index(expected[0]) < run.stdout.index(expected[1])
        assert 'EXPAND decl=buf frame=-80' in run.stdout and 'EXPAND decl=want frame=-80' in run.stdout
        allocations=[line for line in run.stdout.splitlines() if line.startswith('ALLOC')]
        assert len(allocations)==3 and 'size=0' in allocations[-1], allocations
        result['cells'][label]={'command':cmd,'preprocessed_sha256':sha(preprocessed),'allocation_events':allocations,'expected_declaration_events':expected}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('compiler stack trace: 2 cells, 6 allocation events; opposite object orders and late inline helper expansion verified')

if __name__=='__main__':
    main()
