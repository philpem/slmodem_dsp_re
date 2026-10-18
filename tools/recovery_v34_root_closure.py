#!/usr/bin/env python3
"""Diagnostic full native-root body-placement control, retained period profile.

Prediction: expanding all defined native-absent root helpers removes budget
transfer to another artificial boundary. Falsifier: surviving artificial calls,
new unsupported control/extern boundaries, or expansion that fails to recover
native shared dispatch/case structure. Not a production source recommendation.
F11512 seven-macro and whole-closure always_inline controls are prior work;
this tests a complete ordinary-body closure with explicit local return scope.
"""
import re,json,subprocess
from pathlib import Path
def parse_args():
    import argparse
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit',action='store_true')
    parser.add_argument('--period',action='store_true')
    parser.add_argument('--period-roots',action='store_true')
    parser.add_argument('--cell',choices=('hi-outlined','hi-full-root','hi-shared-table2'))
    parser.add_argument('--out',type=Path,default=root/'build/v34-root-closure293')
    parser.add_argument('--baseline-dir',type=Path,default=root/'build/snapshot-dfc-baseline')
    parser.add_argument('--snapshot-object',type=Path,default=root/'build/v34-trnseg4a-case293/frozen/snapshot.o')
    return parser.parse_args()

if __name__=='__main__':
    CLI_ARGS=parse_args()

import recovery_v34_trnseg4a_case as base
import recovery_v34_handshake_boundary as inv
REV='94caa17be36d0c8e53c3eb92827bf68fb874331f'
OUT=base.ROOT/'build/v34-root-closure293'
FILES=('src/pump/v34/V34hshak.c','include/dsplib/v34hstx1_arms.h')
def pinned(path):return subprocess.check_output(['git','show',REV+':'+path],cwd=base.ROOT,text=True)
def masked(s):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:''.join('\n' if c=='\n' else ' ' for c in m[0]),s,flags=re.S)
def closure():
    trace=(base.ROOT/'build/v34-trnseg4a-case293/hi-outlined/V34hshak.c.00.cgraph').read_text();a=trace.index('Initial callgraph:');z=trace.index('Considering ',a);trace=trace[a:z]
    graph={m[1]:m[2].split() for m in re.finditer(r'^([A-Za-z_]\w*):[^\n]*\n  called by:[^\n]*\n  calls:([^\n]*)',trace,re.M)}
    files={p:pinned(p) for p in FILES};definitions={}
    for n in graph:
        for p,s in files.items():
            mask=masked(s);m=re.search(r'(?m)^'+re.escape(n)+r'\s*\(([^;{}]*)\)\s*\{',mask)
            if not m:continue
            start=s.rfind('\n',0,m.start()-1)+1;brace=m.end()-1;depth=1;end=brace+1
            while depth:
                if mask[end]=='{':depth+=1
                elif mask[end]=='}':depth-=1
                end+=1
            definitions[n]={'path':p,'start':start,'end':end,'signature':s[start:m.start()].strip(),'parameters':s[m.start():brace].split('(',1)[1].rsplit(')',1)[0],'body':s[brace+1:end-1]}
    native=set(inv.inspect(base.d.b.BLOB)['defined_text_functions']);seen=set();pending=['v34handshak']
    while pending:
        n=pending.pop()
        if n in seen:continue
        seen.add(n);pending.extend(q for q in graph[n] if q in definitions and q not in native)
    names=seen-{'v34handshak'};assert len(names)==114,(len(names),sorted(names))
    report={'source_pin':REV,'prediction':__doc__,'helpers':sorted(names),'external_boundaries':sorted({q for n in seen for q in graph[n] if q not in seen}),'source_graph':{n:graph[n] for n in sorted(seen)}}
    OUT.mkdir(exist_ok=True);(OUT/'domain.json').write_text(json.dumps(report,indent=2)+'\n')
    return files,definitions,names

def macro(name,d):
    # Rename every local parameter, so an actual argument named like its formal
    # never becomes an uninitialized self-binding. Preserve all ordinary body
    # statements, bug paths and native-symbol calls. Local labels scope exits.
    params=[];bindings=[];body=d['body'];ret=re.sub(r'\b(?:static|inline)\s*','',d['signature']).strip();void=ret=='void';prefix='v34_cb_'+name+'_'
    for i,p in enumerate(d['parameters'].split(',')):
        p=p.strip()
        if p in ('','void'):continue
        m=re.search(r'([A-Za-z_]\w*)$',p);assert m,(name,p);formal=m[1];arg=prefix+'arg'+str(i);local=prefix+formal
        params.append(arg);bindings.append(p[:m.start()]+local+' = ('+arg+');');
        for occurrence in reversed(list(re.finditer(r'\b'+formal+r'\b',masked(body)))):
            if re.search(r'(?:->|\.)\s*$',masked(body[:occurrence.start()])):continue
            body=body[:occurrence.start()]+local+body[occurrence.end():]
    # Strip comments only; string contents are not rewritten by return scanner.
    body=re.sub(r'/\*.*?\*/|//[^\n]*',lambda m:''.join('\n' if c=='\n' else ' ' for c in m[0]),body,flags=re.S)
    mask=masked(body);edits=[]
    for m in re.finditer(r'\breturn\b',mask):
        end=mask.index(';',m.end());expr=body[m.end():end].strip();assert void == (not bool(expr)),(name,ret,expr)
        replacement='do { '+('' if void else prefix+'result = ('+expr+'); ')+'goto '+prefix+'done; } while (0)'
        edits.append((m.start(),end,replacement))
    for a,z,t in reversed(edits):body=body[:a]+t+body[z:]
    if name=='tx1_ts_rates':
        body,n=re.subn(r'#ifdef DSPLIB_V34_TEST_TX_RATE_CAP\n.*?#endif', 'V34_CLOSURE_TEST_CAP('+prefix+'cfg);',body,flags=re.S);assert n==1
    assert not re.search(r'^\s*#',body,re.M),'preprocessor directive inside body '+name
    parts=['({ __label__ '+prefix+'done;']+([] if void else [ret+' '+prefix+'result;'])+bindings+[body,prefix+'done: ;',(' (void)0;' if void else prefix+'result;')+' })']
    bodyname='V34_BODY_'+name
    definition='#define '+bodyname+'('+','.join(params)+') '+(' \\\n'.join('\n'.join(parts).splitlines()))+'\n'
    formals=[re.search(r'([A-Za-z_]\w*)$',p.strip())[1] for p in d['parameters'].split(',') if p.strip() not in ('','void')]
    facade=d['signature']+'\n'+name+'('+d['parameters']+')\n{ '+('' if void else 'return ')+bodyname+'('+','.join(formals)+'); }\n'
    alias='#define '+name+'('+','.join(params)+') '+bodyname+'('+','.join(params)+')\n'
    return definition+facade+alias

def generate():
    files,defs,names=closure();new=dict(files)
    for p,s in files.items():
        edits=[(defs[n]['start'],defs[n]['end'],macro(n,defs[n])) for n in names if defs[n]['path']==p]
        for a,z,t in sorted(edits,reverse=True):s=s[:a]+t+s[z:]
        if p.endswith('v34hstx1_arms.h'):
            s='#ifdef DSPLIB_V34_TEST_TX_RATE_CAP\n#define V34_CLOSURE_TEST_CAP(cfg) do { if ((cfg)->txbits > DSPLIB_V34_TEST_TX_RATE_CAP) (cfg)->txbits = DSPLIB_V34_TEST_TX_RATE_CAP; } while (0)\n#else\n#define V34_CLOSURE_TEST_CAP(cfg) ((void)0)\n#endif\n'+s
        new[p]=s
    cells={'hi-outlined':{'source':files[FILES[0]],'headers':{'dsplib/v34hstx1_arms.h':files[FILES[1]]}},'hi-full-root':{'source':new[FILES[0]],'headers':{'dsplib/v34hstx1_arms.h':new[FILES[1]]}}}
    # Every route through table2 terminates the root call. Preserve the tx
    # argument separately: native tail sometimes rereads the object's txstate.
    rootdef=defs['v34handshak']; tail_names={'t3m_txblock','t3c_txblock'}
    graph=json.loads((OUT/'domain.json').read_text())['source_graph']
    while True:
        old=set(tail_names);tail_names|={n for n,v in graph.items() if any(q in tail_names for q in v)}
        if old==tail_names:break
    contexts=[]
    for owner in sorted(tail_names):
        body=defs[owner]['body']; mask=masked(body)
        for m in re.finditer(r'\b('+'|'.join(sorted(tail_names-{'v34handshak'}))+r')\s*\(',mask):
            depth=1;end=m.end()
            while depth:depth+=(mask[end]=='(')-(mask[end]==')');end+=1
            suffix=mask[end:].strip();tail=bool(re.match(r'^;\s*return\b',suffix)) or suffix==';'
            valued=(m[1]=='t44_det_info_accept' and bool(re.match(r'^!= 0\)\s*return;',suffix)))
            assert tail or valued,(owner,m[1],suffix[:100])
            contexts.append({'owner':owner,'callee':m[1],'source_suffix':body[end:end+100],'terminal':True,'valued_return_checked':valued})
    (OUT/'tail-contexts.json').write_text(json.dumps(contexts,indent=2)+'\n')
    source=new[FILES[0]]; marker='void\nv34handshak(void *vobj)\n{'; assert source.count(marker)==1
    override='#undef t3m_txblock\n#define t3m_txblock(frame_arg, tx_arg) ({ \\\n struct t3m_frame *v34_join_f = (frame_arg); \\\n short v34_join_tx = (tx_arg); \\\n v34_join_frame.obj = v34_join_f->obj; \\\n v34_join_frame.m = v34_join_f->m; \\\n v34_join_frame.rx = v34_join_f->rx; \\\n v34_join_frame.progress = v34_join_f->progress; \\\n v34_join_txstate = v34_join_tx; \\\n goto v34_shared_table2; (void)0; })\n'
    source=source.replace(marker,override+marker+'\n struct t3m_frame v34_join_frame;\n short v34_join_txstate;')
    start=source.index(marker);brace=source.index('{',start);mask=masked(source);depth=1;end=brace+1
    while depth:depth+=(mask[end]=='{')-(mask[end]=='}');end+=1
    source=source[:end-1]+'\n return;\nv34_shared_table2:\n V34_BODY_t3m_txblock(&v34_join_frame, v34_join_txstate);\n return;\n'+source[end-1:]
    cells['hi-shared-table2']={'source':source,'headers':dict(cells['hi-full-root']['headers'])}
    return cells
def period_roots():
    import shutil,os
    stable=base.ROOT/'build/period';sources=list(stable.glob('src_*.o'));assert len(sources)==300
    report=json.loads((OUT/'period-roots.json').read_text()) if (OUT/'period-roots.json').exists() else {}
    for label in ('hi-outlined','hi-full-root','hi-shared-table2'):
        for test in ('t_v34hshak','t_v34hsfsk'):
            folder=OUT/('period-'+label+'-'+test);folder.mkdir(exist_ok=True)
            for path in sources+list(stable.glob('test_harness_*.o'))+[stable/('test_unit_'+test+'.o')]:shutil.copy2(path,folder/path.name)
            selected=folder/'src_pump_v34_V34hshak.o';shutil.copy2(OUT/label/'V34hshak.o',selected);digest=base.sha(selected);fixture=folder/('test_unit_'+test+'.o');fixture_digest=base.sha(fixture)
            command=['make','period','T='+test,'J=4']
            with (folder/'gate.log').open('w') as log:status=subprocess.run(command,cwd=base.ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(base.ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
            assert base.sha(selected)==digest and base.sha(fixture)==fixture_digest,'experiment object overwritten'
            log=(folder/(test+'.run.log')).read_text();key=label+'/'+test
            report[key]={'command':command,'exit':status,'object_sha256':digest,'fixture_sha256':fixture_digest,'checks':sum(map(int,re.findall(r'(\d+) checks',log))),'log':log}
            (OUT/'period-roots.json').write_text(json.dumps(report,indent=2)+'\n')
            print(key,status,report[key]['checks'],flush=True)
            assert status==0 and 'FAIL' not in log and 'PASS ' in log

if __name__=='__main__':
    args=CLI_ARGS;OUT=args.out.resolve()
    base.OUT=OUT;base.REV=REV;base.generate=generate
    base.BASELINE=args.baseline_dir.resolve();base.SNAPSHOT=args.snapshot_object.resolve()
    if args.cell:
        chosen=args.cell;base.generate=lambda:{chosen:generate()[chosen]}
    if args.period_roots:period_roots()
    elif args.period:base.period()
    elif args.audit:base.audit()
    else:base.compile_cells()
