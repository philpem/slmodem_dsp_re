#!/usr/bin/env python3
"""Cross native-witnessed preinitdigital group owners and publication order.

Eight cells: named scrambler/descrambler owners versus direct fields,
retained versus native group-store sequence, TX shell versus callback-field
owner. Prior helper-visibility and operand-spelling domains remain closed.
"""
import json,itertools,re,subprocess,sys
import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data


def variants(path,source):
    if path.endswith('v34digital.c'):return {'baseline':source}
    a,z,fn=d.function(source,'preinitdigital')
    old='''\tobj->scrambler.w[0] = 0;
\tobj->scrambler.w[1] = 0;
\tobj->scrambler.w[2] = 0;
\tobj->scrambler.w[3] = 0;
\tobj->scrambler.nbits = 0x20;

\tobj->descrambler.w[0] = 0;
\tobj->descrambler.w[1] = 0;
\tobj->descrambler.w[2] = 0;
\tobj->descrambler.count = 0;'''
    native='''\tobj->scrambler.w[0] = 0;
\tobj->scrambler.nbits = 0x20;
\tobj->scrambler.w[3] = 0;
\tobj->scrambler.w[2] = 0;
\tobj->scrambler.w[1] = 0;

\tobj->descrambler.count = 0;
\tobj->descrambler.w[0] = 0;
\tobj->descrambler.w[2] = 0;
\tobj->descrambler.w[1] = 0;'''
    assert fn.count(old)==1
    result={}
    for owners,order,field in itertools.product((False,True),repeat=3):
        block=native if order else old
        if owners:
            block=block.replace('obj->scrambler.','s->').replace('obj->descrambler.','r->')
            block='\t{\n\t\tstruct v34_scrambler *s = &obj->scrambler;\n\t\tstruct v34_descrambler *r = &obj->descrambler;\n\n'+'\n'.join('\t'+l if l else l for l in block.splitlines())+'\n\t}'
        f=fn.replace(old,block)
        if field:
            old_owner='\t\tstruct v34_shell *tx = (struct v34_shell *)((char *)obj + V34_SHELL_TX);'
            new_owner='\t\tv34_scramble_fn *tx_scramble =\n\t\t\t&((struct v34_shell *)((char *)obj + V34_SHELL_TX))->scramble;'
            assert f.count(old_owner)==1
            f=f.replace(old_owner,new_owner).replace('tx->scramble =','*tx_scramble =')
        label='baseline' if not any((owners,order,field)) else '%s-%s-%s'%('owners' if owners else 'direct','native' if order else 'ascending','field' if field else 'shell')
        result[label]=source[:a]+f+source[z:]
    assert len(result)==len(set(result.values()))==8
    return result


def graph(text):
    # Restrict to the state/publication tail after all three native memsets.
    tail=text[text.rfind('R_386_PC32 sysdep_memset'):]
    sm=re.search(r'lea\s+0x2a54\(%ebx\),(%e[a-z]+)',tail)
    dm=re.search(r'lea\s+0xe74\(%ebx\),(%e[a-z]+)',tail)
    def native_offsets(match, offsets):
        if not match:return False
        pos=[tail.find('0x%x(%s)'%(n,match[1])) for n in offsets]
        return all(i>=0 for i in pos) and pos==sorted(pos)
    role=tail[tail.find('0x359c(%ebx)'):]
    return {'group_owner_leas':bool(sm and dm),
            'scrambler_native_order':native_offsets(sm,(0x10,0xc,0x8,0x4)),
            'descrambler_native_order':native_offsets(dm,(0xc,0x8,0x4)),
            'direct_tx_publication':role.count('0x2a28(%ebx)')==2,
            'tx_shell_lea_absent':'lea    0x1be0(%ebx)' not in tail}


def disassemble(path):
    return subprocess.check_output(['python3',str(d.ROOT/'tools/dis.py'),str(path),'preinitdigital'],text=True,stderr=subprocess.DEVNULL)


def audit():
    root=d.ROOT/'build'/d.OUT_NAME
    base=root/'v34shell/baseline/candidate.o';old=Object(base).canonical()
    immutable=root/'v34digital/baseline/candidate.o'
    results=json.loads((root/'results.json').read_text())
    report={'source_cells':8,'pair_bodies':len(old[0])+len(Object(immutable).canonical()[0]),'cells':{}}
    for label in results['families']['v34shell']['cells']:
        if label=='baseline':continue
        candidate=root/'v34shell'/label/'candidate.o';cur=Object(candidate).canonical()
        assert set(old[0])==set(cur[0]) and old[1:3]==cur[1:3]
        assert named_data(base)==named_data(candidate)
        assert {n:v for n,v in allocated(base).items() if n!='.text'}=={n:v for n,v in allocated(candidate).items() if n!='.text'}
        changed=[n for n in old[0] if old[0][n]!=cur[0][n]]
        assert changed==['preinitdigital'],(label,'bystander body drift',changed)
        assert old[3]==cur[3],(label,'relocation consumer drift')
        data=results['families']['v34shell']['cells'][label]
        assert not data['gains'] and not data['losses'],(label,'exact membership drift')
        report['cells'][label]={'changed_bodies':changed,'verdict':data['verdicts']['preinitdigital'],
            'exact_gains':data['gains'],'exact_losses':data['losses'],
            'data_bindings_bystander_consumers_equal':True,
            'graph':graph(disassemble(candidate))}
    report['reference_graph']=graph(disassemble(d.b.BLOB))
    report['baseline_graph']=graph(disassemble(base))
    assert all(report['reference_graph'].values())
    selected=report['cells']['owners-native-field']['graph']
    assert all(selected.values()),selected
    assert not report['baseline_graph']['group_owner_leas']
    assert not report['baseline_graph']['direct_tx_publication']
    assert report['cells']['owners-ascending-field']['graph']['group_owner_leas']
    assert not report['cells']['owners-ascending-field']['graph']['scrambler_native_order']
    assert not report['cells']['direct-native-field']['graph']['group_owner_leas']
    wrong=disassemble(root/'v34shell/owners-native-field/candidate.o').replace('0x2a54(%ebx)','0x2a58(%ebx)')
    assert not graph(wrong)['group_owner_leas'],'wrong-owner negative did not fire'
    globals_=results['families']['v34shell']['cells']['baseline']['globals']
    report['defined_globals']=len(globals_)
    report['external_imports']=sorted(s.name for s in Object(base).symbols if s['st_info']['bind']!='STB_LOCAL' and s['st_shndx']=='SHN_UNDEF')
    report['matrix_valid_compiles']=9
    report['baseline_raw_controls']=2
    report['candidate_function_verdicts']=7*report['pair_bodies']
    report['unchanged_bystander_verdicts']=7*12
    report['named_data_records']=len(named_data(base))
    report['unchanged_bystanders_per_candidate']=12
    report['controls']={'baseline_owner_and_publication_negatives':True,
                        'ascending_order_negative':True,'direct_owner_negative':True,
                        'wrong_owner_mutation_negative':True}
    (root/'publication-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('publication audit:',len(report['cells']),'candidates;',report['pair_bodies'],'pair functions')
    for label,row in report['cells'].items():print(label,row['verdict'],row['changed_bodies'])


def production_control():
    root=d.ROOT/'build'/d.OUT_NAME
    results=json.loads((root/'results.json').read_text())
    selected=results['families']['v34shell']['cells']['owners-native-field']
    out=root/'v34shell/production-control';out.mkdir(exist_ok=True)
    source=d.ROOT/'src/pump/v34/v34shell.c'
    (out/'v34shell.c').write_bytes(source.read_bytes())
    command=[s.replace('/v34shell/owners-native-field','/v34shell/production-control') for s in selected['command']]
    with (out/'compile.log').open('w') as log:
        subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert (out/'candidate.o').read_bytes()==(root/'v34shell/owners-native-field/candidate.o').read_bytes(), 'production object drift'
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    print('production-control: complete object raw-identical to selected cell')


if __name__=='__main__':
    d.REV='c118236196b1e6a6bd4f7aeafd6458cea1ad544c';d.OUT_NAME='preinit-publication'
    d.SOURCE_PATHS=('src/pump/v34/v34shell.c','src/pump/v34/v34digital.c')
    d.DUMP_FLAGS=('-da','-v','-save-temps');d.variants=variants
    if sys.argv[1:]==['--production-control']:
        production_control()
    else:
        d.main();audit()
