#!/usr/bin/env python3
"""Observe installed C-frontend reload choices and require raw object equality.

Input: a complete-TU playbook reproduction with -v -save-temps -da.
The GDB script pins three hooks by instruction bytes in the extracted cc1.
An absent target is a failed observation, even when compilation succeeds.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
_script_path=sys.path[:]
sys.path[:]=[p for p in sys.path if Path(p or '.').resolve()!=Path(__file__).resolve().parent]
import dis
sys.path[:]=_script_path
import experiment_toolchain as tc
ROOT=Path(__file__).resolve().parents[1]


def validate(trace, run_negative=True):
    assert trace['exits']==[0] and not trace['errors'] and not trace['pending']
    records=trace['records'];alternatives=trace['alternatives']
    assert len(records)==9 and len(alternatives)==14
    assert {r['uid'] for r in records}=={56,187,189}
    target=[r for r in records if r['uid']==187]
    assert len(target)==3 and sum(r['replace']==1 for r in target)==1
    for row in target:
        assert row['constraints']==['=f','%0','fm','']
        assert row['n_alternatives']==1 and row['goal_alternative_number']==0
        assert row['goal_alternative_swapped']==0 and row['best']==8
        assert row['operands'][0]['regno']==8 and row['operands'][2]['regno']==9
        assert row['operands'][1]['code']=='MEM'
        assert len(row['reloads'])==1
        reload=row['reloads'][0]
        assert reload['in']['code']=='MEM' and reload['out']['regno']==8
        assert reload['class']=='FLOAT_REGS' and reload['optional']==0
    scores=[r for r in alternatives if r['uid']==187]
    assert len(scores)==6
    assert all(r['losers']==1 and r['reject']==2 and r['score']==8 and r['bad']==0 for r in scores)
    assert [r['swapped'] for r in scores]==[0,1,0,1,0,1]
    # Same machine pattern with an already matching destructive input: zero
    # is preferred to fourteen. These controls reject a tie-only detector.
    for uid in (56,189):
        control=[r for r in alternatives if r['uid']==uid]
        assert [r['score'] for r in control[:2]]==[14,0]
    if run_negative:
        wrong=json.loads(json.dumps(trace))
        next(r for r in wrong['alternatives'] if r['uid']==187)['score']=7
        try:
            validate(wrong, False)
        except AssertionError:
            pass
        else:
            raise AssertionError('cost mutation negative did not fire')


def validate_frame(stack, prologue, run_negative=True):
    assert stack['exit_codes']==[0] and not stack['errors'] and not stack['pending']
    events=stack['events'];frames=stack['frames']
    assert [(e['mode'],e['size'],e['frame_before'],e['frame_after']) for e in events]==[
        ('SImode',4,0,-4),('BLKmode',0,-4,-4),('SImode',4,-4,-8),('BLKmode',0,-8,-8)]
    assert [e['caller_values'][0]['available_values']['i'] for e in events if e['size']]==[69,76]
    assert len(frames)==19
    for event in frames:
        assert event['stack_alignment_needed']==event['preferred_stack_boundary']==32
        layout=event['layout']
        assert event['locals_size'] in (4,8) and event['outgoing_args_size']==0
        assert not event['frame_pointer_needed'] and layout['nregs']==4
        assert layout['padding1']==layout['padding2']==layout['outgoing_arguments_size']==0
        assert layout['to_allocate']==event['locals_size']
        assert layout['stack_pointer_offset']==20+event['locals_size']
        assert layout['save_regs_using_mov']==0
    assert frames[-1]['reload_completed'] and frames[-1]['locals_size']==8
    assert len(prologue['frames'])==19 and len(prologue['costs'])==1
    assert prologue['costs'][0]['expensive']==1
    assert prologue['frames'][0]['cache_before']['use_fast_prologue_epilogue_nregs']==-1
    assert all(e['cache_after']=={'use_fast_prologue_epilogue':0,'use_fast_prologue_epilogue_nregs':4} for e in prologue['frames'])
    if run_negative:
        wrong=json.loads(json.dumps(stack));wrong['frames'][-1]['layout']['padding2']=4
        try:
            validate_frame(wrong, prologue, False)
        except AssertionError:
            pass
        else:
            raise AssertionError('padding mutation negative did not fire')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reproduction-dir',required=True,type=Path)
    ap.add_argument('--output-dir',required=True,type=Path)
    ap.add_argument('--target',default='EchoCanceler')
    ap.add_argument('--stack-observation',type=Path)
    ap.add_argument('--prologue-observation',type=Path)
    args=ap.parse_args();source=args.reproduction_dir.resolve();out=args.output_dir.resolve();out.mkdir(exist_ok=True)
    prior=json.loads((source/'results.json').read_text());image=prior['config'].splitlines()[0].split(' ',1)[1]
    inputs=source/'Fdspkrnl/baseline';cell=prior['families']['Fdspkrnl']['cells']['baseline']
    assert cell['baseline_reproduced'], 'no raw full-TU baseline control'
    assert hashlib.sha256((inputs/'candidate.o').read_bytes()).hexdigest()==cell['object_hash']
    assert tc.REPRODUCE_BUGS in shlex.split(cell['command'][-1]), 'missing reproduction define'
    lines=(inputs/'compile.log').read_text().splitlines()
    compiler=shlex.split(next(s for s in lines if ' -fpreprocessed ' in s))
    assembler=shlex.split(next(s for s in lines if '/bin/as ' in s))
    selected=subprocess.check_output(['docker','run','--rm','--platform','linux/386',image,'cat',compiler[0]])
    cc1=out/'cc1';cc1.write_bytes(selected);cc1.chmod(0o755)
    shutil.copy2(inputs/'Fdspkrnl.i',out/'Fdspkrnl.i');compiler[0]=str(cc1);compiler[compiler.index('-o')+1]='observed.s'
    debugger=['gdb','-q','-batch','-ex','set pagination off','-ex','source '+str(ROOT/'tools/gcc3_echo_reload_gdb.py'),'--args']+compiler
    with (out/'gdb.log').open('w') as log:
        subprocess.run(debugger,cwd=out,env=dict(os.environ,GCC3_RELOAD_TARGET=args.target),stdout=log,stderr=subprocess.STDOUT,check=True)
    assembler[assembler.index('-o')+1]='/work/observed.o';assembler[-1]='observed.s'
    assemble=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd /work && '+shlex.join(assembler)]
    with (out/'as.log').open('w') as log:subprocess.run(assemble,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert (inputs/'candidate.o').read_bytes()==(out/'observed.o').read_bytes(), 'observer changed emitted object'
    trace=json.loads((out/'reload-observe.json').read_text());validate(trace)
    report={'revision':prior['revision'],'config':prior['config'],'compiler_sha256':hashlib.sha256(selected).hexdigest(),
            'object_sha256':cell['object_hash'],'raw_object_unchanged':True,'commands':[debugger,assemble],
            'target':args.target,'trace':trace,'controls':{'three_target_choices':True,'six_equal_cost_orientations':True,
            'two_non_tie_uid_controls':True,'cost_mutation_rejected':True}}
    assert bool(args.stack_observation)==bool(args.prologue_observation), 'provide both frame observations'
    if args.stack_observation:
        stack=json.loads((args.stack_observation/'results.json').read_text())
        prologue=json.loads((args.prologue_observation/'results.json').read_text())
        assert stack['revision']==prologue['revision']==prior['revision']
        assert stack['compiler_sha256']==prologue['cells']['baseline']['compiler_sha256']==report['compiler_sha256']
        assert stack['cells']['baseline']['object_sha256']==report['object_sha256']
        assert stack['cells']['baseline']['raw_object_unchanged'] and prologue['cells']['baseline']['raw_object_unchanged']
        validate_frame(stack['cells']['baseline']['trace'],prologue['cells']['baseline']['trace'])
        report['frame_controls']={'allocations':4,'layouts':19,'cost_calls':1,'padding_mutation_rejected':True,
                                  'locals':8,'padding':0,'outgoing':0,'preferred_boundary_bits':32}
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('reload observation: 9 choices; 14 orientations; 3/3 target ties; 2/2 non-tie controls; raw complete object unchanged')


if __name__=='__main__':main()
