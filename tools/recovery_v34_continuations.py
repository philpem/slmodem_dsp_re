#!/usr/bin/env python3
"""Ordinary V34 terminal continuations with one physical body home.

Three cells: unchanged source, ordinary terminal continuations, and those
continuations with nineteen ordinary table1 cases. Retain other leaf helpers
to expose residual inline decisions.
Prediction: exactly one 70-entry dispatch and one instance of each shared
terminal diagnostic, without GNU statement-expression body macros.
Falsifier: duplicated dispatch, skipped post-call work or differential failure.
"""
import argparse,json,re,subprocess,shutil,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REV='28ccee0669a7f02a1663736b10900d239928b824'
OUT=ROOT/'build/v34-continuations-current'
def parse_args():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,default=OUT)
    p.add_argument('--revision',default=REV,help='source/header commit; override for historical replay')
    p.add_argument('--canonical-object',type=Path,help='archived unchanged baseline object; required with --config for a new output')
    p.add_argument('--config',type=Path,help='archived complete baseline config; existing outputs reuse their frozen inputs')
    p.add_argument('--audit',action='store_true')
    p.add_argument('--period',action='store_true')
    p.add_argument('--verify-installed',action='store_true',help='after compilation, verify installed production/fixture raw identity')
    p.add_argument('--cell',choices=('hi-outlined','hi-continuations','hi-table1'))
    return p.parse_args()
if __name__=='__main__':CLI_ARGS=parse_args()
import recovery_v34_root_closure as old
import recovery_v34_trnseg4a_case as base
import recovery_v34_handshake_boundary as inv

def ret_body(body):
    mask=old.masked(body);edits=[]
    for m in re.finditer(r'\breturn\b',mask):
        end=mask.index(';',m.end());edits.append((m.start(),end,'V34_CONT_RETURN('+body[m.end():end].strip()+')'))
    for a,z,t in reversed(edits):body=body[:a]+t+body[z:]
    return body

def params(d):
    return [(p.strip(),re.search(r'([A-Za-z_]\w*)$',p.strip())[1]) for p in d['parameters'].split(',') if p.strip() not in ('','void')]

def generate():
    old.REV=REV;old.OUT=OUT;files,defs,names=old.closure()
    micro47=files[old.FILES[0]][defs['t3m_micro47']['start']:defs['t3m_micro47']['end']]
    normal_guard='t3m_info0_carrier' in files[old.FILES[0]]
    graph=json.loads((OUT/'domain.json').read_text())['source_graph'];ancestry={'t3c_txblock','t3m_txblock'}
    while True:
        before=set(ancestry);ancestry|={n for n,v in graph.items() if any(q in ancestry for q in v)}
        if before==ancestry:break
    assert len(ancestry)==28
    valued='t44_det_info_accept';initializer='t3m_frame_init'
    labels=(ancestry-{'v34handshak',valued})|{'t3m_tail'}
    assert len(labels)==27 and all(defs[n]['signature']=='static void' for n in labels)
    physical=labels|{valued,initializer};headers={'dsplib/v34hstx1_arms.h':files[old.FILES[1]]};source=files[old.FILES[0]]
    assert all(defs[n]['path']==old.FILES[0] for n in physical)
    bodies={n:ret_body(defs[n]['body']) for n in physical}
    # The single valued helper normally returns to its caller. Its terminal
    # dispatch path still jumps to the same root continuation as every other.
    needle='if (t44_det_info_accept(obj, rec, next) != 0)\n\t\t\t\treturn;'
    replacement='''{
                int v34_accept_result;
                {
                    short count2 = next;
#undef V34_CONT_RETURN
#define V34_CONT_RETURN(value) do { v34_accept_result = (value); goto v34_accept_done; } while (0)
#include "dsplib/v34_continuations/t44_det_info_accept.inc"
#undef V34_CONT_RETURN
v34_accept_done:
                    ;
                }
                if (v34_accept_result != 0)
                    V34_CONT_RETURN();
            }'''
    # Nested include needs to restore the containing void continuation's exit.
    replacement=replacement.replace('#undef V34_CONT_RETURN\nv34_accept_done:', '#undef V34_CONT_RETURN\n#define V34_CONT_RETURN(value) return value\nv34_accept_done:')
    assert bodies['t44_micro_det_info'].count(needle)==0 # returns already parameterised
    needle=needle.replace('return;','V34_CONT_RETURN();')
    assert bodies['t44_micro_det_info'].count(needle)==1
    bodies['t44_micro_det_info']=bodies['t44_micro_det_info'].replace(needle,replacement)
    # Root/helper facades use the same physical bodies, with ordinary returns.
    edits=[]
    for n in physical:
        d=defs[n];include='dsplib/v34_continuations/'+n+'.inc';headers[include]=bodies[n]+'\n'
        facade=d['signature']+'\n'+n+'('+d['parameters']+')\n{\n#define V34_CONT_RETURN(value) return value\n#include "'+include+'"\n#undef V34_CONT_RETURN\n}\n'
        # The special nested inclusion above belongs only to the root path:
        # keep normal facade call/return semantics via conditional selection.
        if n=='t44_micro_det_info':
            headers[include]=headers[include].replace(replacement,'#ifdef V34_ROOT_CONTINUATIONS\n'+replacement+'\n#else\n'+needle+'\n#endif')
        edits.append((d['start'],d['end'],facade))
    for a,z,t in sorted(edits,reverse=True):source=source[:a]+t+source[z:]
    declarations=[];bindings=[];blocks=[]
    for n in sorted(labels):
        d=defs[n];arguments=[];temps=[];assign=[];aliases=[]
        for i,(typed,name) in enumerate(params(d)):
            context='v34_ctx_'+n+'_'+name;arg='v34_arg_'+n+'_'+str(i);temp='v34_tmp_'+n+'_'+str(i)
            arguments.append(arg);temps.append(re.sub(r'\b'+name+r'$',temp,typed)+' = ('+arg+');')
            if name=='f' and n in ('t3m_tail','t3m_txblock'):
                declarations.append('struct t3m_frame '+context+';')
                assign.extend(context+'.'+field+' = '+temp+'->'+field+';' for field in ('obj','m','rx','progress'))
                aliases.append(typed+' = &'+context+';')
            else:
                declarations.append(re.sub(r'\b'+name+r'$',context,typed)+';');assign.append(context+' = '+temp+';');aliases.append(typed+' = '+context+';')
        binding_lines=['#define '+n+'('+','.join(arguments)+')', '    do {']+['        '+line for line in temps+assign]+['        goto v34_cont_'+n+';', '    } while (0)']
        bindings.append((' \\\n'.join(binding_lines)))
        blocks.append('v34_cont_'+n+':\n{\n'+'\n'.join(aliases)+'\n#define V34_CONT_RETURN(value) return value\n#include "dsplib/v34_continuations/'+n+'.inc"\n#undef V34_CONT_RETURN\n}\nreturn;\n')
    # Initialisation is nonterminal: insert an ordinary scoped body at each
    # actual root use; a macro would merely reproduce a function boundary.
    def inline_init(text):
        mask=old.masked(text);edits=[]
        for m in re.finditer(r'\bt3m_frame_init\(([^;]*)\);',mask):
            args=text[m.start():m.end()].split('(',1)[1].rsplit(')',1)[0].split(',');assert len(args)==2
            init='{ struct t3m_frame *v34_init_frame = '+args[0].strip()+'; struct v34_object *v34_init_obj = '+args[1].strip()+'; { struct t3m_frame *f = v34_init_frame; struct v34_object *obj = v34_init_obj;\n#include "dsplib/v34_continuations/t3m_frame_init.inc"\n} }'
            edits.append((m.start(),m.end(),init))
        for a,z,t in reversed(edits):text=text[:a]+t+text[z:]
        return text
    headers['dsplib/v34_continuations/t3c_txblock.inc']=inline_init(headers['dsplib/v34_continuations/t3c_txblock.inc'])
    # The facade also consumes this body; initialisation is ordinary in both.
    marker='void\nv34handshak(void *vobj)\n{';assert source.count(marker)==1
    start=source.index(marker);brace=source.index('{',start);mask=old.masked(source);depth=1;end=brace+1
    while depth:depth+=(mask[end]=='{')-(mask[end]=='}');end+=1
    rootbody=inline_init(source[brace+1:end-1]);rootbody='\n'+'\n'.join(declarations)+'\n'+rootbody+'\nreturn;\n'+'\n'.join(blocks)
    prefix='#define V34_ROOT_CONTINUATIONS 1\n'+'\n'.join(bindings)+'\n'
    suffix='\n#undef V34_ROOT_CONTINUATIONS\n'+'\n'.join('#undef '+n for n in sorted(labels))+'\n'
    source=source[:start]+prefix+source[start:brace+1]+rootbody+source[end-1:end]+suffix+source[end:]
    assert '({' not in source and all('({' not in t for t in headers.values()),'GNU statement expression introduced'
    domain={'source_pin':REV,'prediction':__doc__,'terminal_ancestry':sorted(ancestry),'ordinary_labels':sorted(labels),'physical_bodies':sorted(physical),'valued_helper':valued,'initializer':initializer}
    (OUT/'continuation-domain.json').write_text(json.dumps(domain,indent=2)+'\n')
    cells={'hi-outlined':{'source':files[old.FILES[0]],'headers':{'dsplib/v34hstx1_arms.h':files[old.FILES[1]]}},'hi-continuations':{'source':source,'headers':headers}}
    # Cross the terminal design with ordinary table1 cases. Include every arm,
    # so another arm cannot merely take the displaced inline-budget slot.
    arms=sorted(set(re.findall(r'\b(v34tx1_\w+)\(obj\);',defs['v34handshak']['body'])))
    assert len(arms)==19
    tableheaders=dict(headers);tableheader=tableheaders['dsplib/v34hstx1_arms.h'];table_source=source;edits=[]
    for n in arms:
        d=defs[n];assert d['path']==old.FILES[1] and d['parameters']=='void *objp'
        include='dsplib/v34_continuations/'+n+'.inc';tableheaders[include]=ret_body(d['body']).replace('V34_CONT_RETURN','V34_TX_CASE_RETURN')+'\n'
        facade=d['signature']+'\n'+n+'('+d['parameters']+')\n{\n#define V34_TX_CASE_RETURN(value) return (value)\n#include "'+include+'"\n#undef V34_TX_CASE_RETURN\n}\n'
        edits.append((d['start'],d['end'],facade))
        call='\t\t\t'+n+'(obj);';assert table_source.count(call)==1
        ordinary='            {\n                void *objp = obj;\n#define V34_TX_CASE_RETURN(value) do { (void)(value); goto v34_table1_continue; } while (0)\n#include "'+include+'"\n#undef V34_TX_CASE_RETURN\n            }'
        table_source=table_source.replace(call,ordinary)
    for a,z,t in sorted(edits,reverse=True):tableheader=tableheader[:a]+t+tableheader[z:]
    tableheaders['dsplib/v34hstx1_arms.h']=tableheader
    bottom='\t\t}\n\n\t\t/*\n\t\t * AND NOTHING IS DISPATCHED'
    assert table_source.count(bottom)==1
    table_source=table_source.replace(bottom,'\t\t}\nv34_table1_continue:\n        ;\n\n\t\t/*\n\t\t * AND NOTHING IS DISPATCHED')
    cells['hi-table1']={'source':table_source,'headers':tableheaders}
    for cell in cells.values():
        for path,text in cell['headers'].items():
            if path.endswith('.inc'):cell['headers'][path]=text.rstrip()+'\n'
    domain['ordinary_table1_arms']=arms
    for cell in cells.values():
        assert micro47 in cell['source'],'micro47 normal/BUGS predicate changed'
        if normal_guard:
            helper=files[old.FILES[0]].split('#ifndef DSPLIB_REPRODUCE_BUGS\n/*\n * D77:',1)[1].split('#endif',1)[0]
            assert helper in cell['source'] and '&& t3m_info0_carrier(f->rx->burst)' in cell['source']
    domain['normal_guard_preserved']=normal_guard
    domain['micro47_sha256']=__import__('hashlib').sha256(micro47.encode()).hexdigest()
    (OUT/'continuation-domain.json').write_text(json.dumps(domain,indent=2)+'\n')
    return cells

def period():
    stable=ROOT/'build/period';sources=list(stable.glob('src_*.o'));assert len(sources)==300
    roots=json.loads((OUT/'period-roots.json').read_text()) if (OUT/'period-roots.json').exists() else {}
    direct=json.loads((OUT/'period.json').read_text()) if (OUT/'period.json').exists() else {}
    for label in base.generate():
        for test in ('t_v34hstx1','t_v34hshak','t_v34hsfsk'):
            folder=OUT/('period-'+label+('' if test=='t_v34hstx1' else '-'+test));folder.mkdir(exist_ok=True)
            for path in sources+list(stable.glob('test_harness_*.o')):shutil.copy2(path,folder/path.name)
            obj=folder/'src_pump_v34_V34hshak.o';shutil.copy2(OUT/label/'V34hshak.o',obj)
            fixture=folder/('test_unit_'+test+'.o')
            fixture_input=OUT/label/'fixture.o' if test=='t_v34hstx1' else stable/fixture.name
            shutil.copy2(fixture_input,fixture);hashes=(base.sha(obj),base.sha(fixture))
            # These are already-compiled experiment inputs. In particular an
            # adopted source must not replace the historical baseline cell.
            obj.touch();fixture.touch()
            command=['make','period','T='+test,'J=4']
            with (folder/'gate.log').open('w') as log:status=subprocess.run(command,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
            assert hashes==(base.sha(obj),base.sha(fixture)),'experiment object overwritten'
            text=(folder/(test+'.run.log')).read_text()
            row={'command':command,'exit':status,'object_sha256':hashes[0],'fixture_sha256':hashes[1],'checks':sum(map(int,re.findall(r'(\d+) checks',text)))}
            if test=='t_v34hstx1':
                row['run_log']=text;row['boundary']='Existing direct-arm component probes plus reference once-per-block tail; synthetic states, not modem lifecycle evidence.'
                direct[label]=row;(OUT/'period.json').write_text(json.dumps(direct,indent=2)+'\n')
            else:
                row['log']=text;roots[label+'/'+test]=row;(OUT/'period-roots.json').write_text(json.dumps(roots,indent=2)+'\n')
            print(label,test,status,row['checks'],flush=True)
            assert status==0 and 'FAIL' not in text and 'PASS ' in text

def verify_installed():
    import shlex
    folder=OUT/'installed';folder.mkdir(exist_ok=True)
    config=(OUT/'frozen/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags=['-I/src/include' if f=='-Iinclude' else '/src/tools/toolchain/period_compat.h' if f=='tools/toolchain/period_compat.h' else f for f in flags]+['-v']
    expected=json.loads((ROOT/'build/v34-continuations-current/verified-fixture-provenance.json').read_text())['hi-table1']
    report={'expected':expected,'commands':[],'objects':{}}
    for name,source,extra,key in [('V34hshak.o','src/pump/v34/V34hshak.c',[],'object_sha256'),('fixture.o','test/unit/t_v34hstx1.c',['-I/src/test/harness','-std=gnu99'],'direct_fixture_sha256')]:
        command=base.d.tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c',base.d.tc.compile_shell(base.d.tc.GENTOO_COMPILER_PATH,flags+extra,'/work/installed/'+name,source)]
        base.run(command,folder,name+'-compile');report['commands'].append(command)
        path=folder/name;assert base.sha(path)==expected[key],('installed raw identity',name,base.sha(path),expected[key])
        assert path.read_bytes()==(OUT/'hi-table1'/name).read_bytes(),('candidate/installed mismatch',name)
        report['objects'][name]={'sha256':base.sha(path),'validated_object_raw_equal':True,'candidate_raw_equal':True}
    (OUT/'installed-identity.json').write_text(json.dumps(report,indent=2)+'\n')
    print('installed production and direct fixture raw-identical to validated candidate',flush=True)

def prepare_baseline(canonical=None,config=None):
    folder=OUT/'canonical-baseline';manifest=folder/'manifest.json'
    if manifest.exists():
        record=json.loads(manifest.read_text())
        assert record['source_pin']==REV,'frozen source pin differs; use a separate output'
        assert base.sha(folder/'src_pump_v34_V34hshak.c.o')==record['object_sha256'],'frozen baseline object drift'
        assert base.sha(folder/'.build-config')==record['config_sha256'],'frozen baseline config drift'
        # Explicit alternate locations may verify identical archived bytes.
        # Preserve the original manifest paths and their recorded provenance.
        if canonical is not None:assert canonical.read_bytes()==(folder/'src_pump_v34_V34hshak.c.o').read_bytes(),'explicit object differs from frozen baseline'
        if config is not None:assert config.read_bytes()==(folder/'.build-config').read_bytes(),'explicit config differs from frozen baseline'
    else:
        if canonical is None or config is None:
            raise ValueError('new output requires --canonical-object and --config for the archived unchanged baseline; existing outputs reuse frozen inputs')
        folder.mkdir(parents=True,exist_ok=True)
        names={'.build-config':config,'src_pump_v34_V34hshak.c.o':canonical}
        for name,path in names.items():
            target=folder/name
            if target.exists():assert target.read_bytes()==path.read_bytes(),'canonical input drift '+name
            else:shutil.copy2(path,target)
        record={'source_pin':REV,'canonical_input':str(canonical),'config_input':str(config),'object_sha256':base.sha(folder/'src_pump_v34_V34hshak.c.o'),'config_sha256':base.sha(folder/'.build-config')}
        manifest.write_text(json.dumps(record,indent=2)+'\n')
    base.BASELINE=folder;base.SNAPSHOT=folder/'src_pump_v34_V34hshak.c.o'

if __name__=='__main__':
    args=CLI_ARGS;OUT=args.out.resolve();REV=subprocess.check_output(['git','rev-parse',args.revision+'^{commit}'],cwd=ROOT,text=True).strip()
    base.OUT=OUT;base.REV=REV;base.generate=generate;base.__doc__=__doc__
    try:
        prepare_baseline(args.canonical_object.resolve() if args.canonical_object else None,args.config.resolve() if args.config else None)
    except ValueError as error:
        raise SystemExit(str(error))
    if args.cell:
        chosen=args.cell;base.generate=lambda:{chosen:generate()[chosen]}
    if args.period:period()
    elif args.audit:base.audit()
    else:base.compile_cells()
    if args.verify_installed:verify_installed()
