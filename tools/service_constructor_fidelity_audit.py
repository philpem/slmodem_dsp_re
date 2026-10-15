#!/usr/bin/env python3
"""Audit thirteen service controls and their explicit nontext changes."""
from collections import Counter
import hashlib
import json
import subprocess
import playbook_small_patterns as d
from helper_caller_audit import inspect as named
from gcc3_value_carriers_audit import inspect, function_chunk
from gcc3_adid_first_study_audit import masked
from narrow_call_boundary_screen import bodies


def strings(path):
    from elftools.elf.elffile import ELFFile
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        return Counter(word for sec in elf.iter_sections() if sec.name.startswith('.rodata.str')
                       for word in sec.data().split(b'\0') if word)


def main():
    root = d.ROOT / 'build/service-constructor-fidelity'
    ledger = json.loads((root/'results.json').read_text())
    report = {}
    total = verdicts = 0
    for family, record in ledger['families'].items():
        baseline = root/family/'baseline/candidate.o'
        assert baseline.read_bytes() == (root/family/'retained.o').read_bytes()
        base, names = inspect(baseline), named(baseline)
        rows = {}
        for label, entry in record['cells'].items():
            path = root/family/label/'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
            assert hashlib.sha256((path.parent/(family+'.c')).read_bytes()).hexdigest() == entry['source_hash']
            now, current = inspect(path), named(path)
            removed = set(names['objects']) - set(current['objects'])
            assert removed == ({'dpw_rate_pairs'} if label=='literal-shared-failure' else
                               {'banks.0'} if label.startswith('original-switch-map') else set())
            assert current['records'] == {k:v for k,v in names['records'].items() if k not in removed}
            allowed_objects = {'banks.0'} if label.startswith('corrected-bank-table') else set()
            ordinary = lambda data: {k:v for k,v in data['objects'].items() if k not in removed|allowed_objects}
            assert ordinary(current) == ordinary(names), (family,label,'ordinary named data')
            assert now['nobits'] == base['nobits'] and strings(path) == strings(baseline)
            changes = {k:[base['allocated'].get(k),now['allocated'].get(k)]
                       for k in set(base['allocated'])|set(now['allocated'])
                       if base['allocated'].get(k)!=now['allocated'].get(k)}
            assert set(changes) <= ({'.rodata'} if removed else set()), (family,label,changes)
            reloc_changes = now['relocations'] != base['relocations']
            if not removed and not allowed_objects:
                assert not reloc_changes
            if family == 'dp_wrapper':
                allowed = {'dp_wrapper_create'}
                if removed:
                    assert len(bytes.fromhex(base['allocated']['.rodata'])) == 72
                    assert '.rodata' not in now['allocated']
                    assert not now['relocations'] and not base['relocations']
            else:
                allowed = set()
                if label.startswith('original-switch-map'): allowed.add('cadence_create')
                if 'interior-on' in label: allowed.add('cadence_progress')
                if allowed_objects:
                    a=names['objects']['banks.0'];b=current['objects']['banks.0']
                    assert a['bytes']==b['bytes'] and a['offset']==b['offset']
                    changed={k for k in set(a['relocations'])|set(b['relocations']) if a['relocations'].get(k)!=b['relocations'].get(k)}
                    assert changed=={144,148,152,168,172,176}, changed
                    for offset,bank in [(144,'100_550'),(168,'350_500')]:
                        for index,field in enumerate(('a','b','scales')):
                            assert b['relocations'][offset+4*index]==['R_386_32',('symbol','Filter_'+bank+'_'+field,0)]
                if removed:
                    assert len(bytes.fromhex(base['allocated']['.rodata']))==224
                    assert len(bytes.fromhex(now['allocated']['.rodata']))==52
                    edges=[r for r in now['relocations'] if r[0]=='.rodata' and r[3][0]=='audited-code-destination']
                    assert len(edges)==8 and all(r[3][1]=='cadence_create' for r in edges)
                    destinations=[r[3][2] for r in edges]
                    assert destinations[0]==destinations[2]
                    assert destinations[1]==destinations[4]==destinations[5]
                    assert len(set(destinations))==5
                    assert now['relocations'][:5]==base['relocations'][:5]
            changed = [n for n in entry['functions'] if masked(path,n)!=masked(baseline,n)]
            assert set(changed)==(allowed if label!='baseline' else set())
            assert not entry.get('gains') and not entry.get('losses')
            for name in changed:
                edges=lambda p:Counter(r['target'] for r in bodies(p)[name] if 'target' in r)
                assert edges(path)==edges(baseline), (family,label,name,'direct calls')
            rows[label]={'changed_bodies':changed,'removed_objects':sorted(removed),
                         'changed_data_objects':sorted(allowed_objects),'allocated_deltas':changes,
                         'nontext_relocations_changed':reloc_changes,'verdicts':entry['verdicts'],
                         'metadata_named_data_calls_bystanders':'pass'}
            total+=1;verdicts+=len(entry['verdicts'])
        report[family]=rows
    assert (total,verdicts)==(9,33)
    diagnostic=d.ROOT/'build/cadence-diagnostic-owner'
    rec=json.loads((diagnostic/'results.json').read_text())['families']['Cadence']
    baseline=diagnostic/'Cadence/baseline/candidate.o'
    assert baseline.read_bytes()==(diagnostic/'Cadence/retained.o').read_bytes()
    base=inspect(baseline);names=named(baseline);rows={}
    for label,entry in rec['cells'].items():
        path=diagnostic/'Cadence'/label/'candidate.o'
        now=inspect(path)
        assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['object_hash']
        assert now['records']==base['records'] and now['allocated']==base['allocated']
        assert now['nobits']==base['nobits'] and named(path)['objects']==names['objects']
        assert strings(path)==strings(baseline)
        # Only the eight internal switch destination offsets may move.
        code=lambda data:[r for r in data['relocations'] if r[3][0]=='audited-code-destination']
        ordinary=lambda data:[r for r in data['relocations'] if r[3][0]!='audited-code-destination']
        assert ordinary(now)==ordinary(base)
        edges=code(now);assert len(edges)==8 and all(r[3][1]=='cadence_create' for r in edges)
        changed=[n for n in entry['functions'] if masked(path,n)!=masked(baseline,n)]
        assert set(changed)==({'cadence_create'} if label!='baseline' else set())
        assert not entry.get('gains') and not entry.get('losses')
        calls=lambda p:Counter(r['target'] for r in bodies(p)['cadence_create'] if 'target'in r)
        assert calls(path)==calls(baseline)
        nodes=function_chunk(path.parent/'Cadence.c.02.sibling','cadence_create').split('\n\n')
        last=max(i for i,n in enumerate(nodes) if '("dsplibs_debug_printf")' in n)
        reads=lambda field:[i for i,n in enumerate(nodes) if '.'+field+'+0' in n and '(set (reg' in n]
        tone=reads('tone');w6=reads('w6')
        assert len(tone)==2 and len(w6)==1, (label,tone,w6)
        assert (tone[-1]>last)==label.startswith('working-tone-late-')
        assert (w6[0]>last)==(label=='working-tone-late-descriptor')
        rows[label]={'changed_bodies':changed,'verdicts':entry['verdicts'],
                     'last_report_call_node':last,'descriptor_tone_read_nodes':tone,'descriptor_w6_read_nodes':w6,
                     'metadata_data_calls_bystanders':'pass; eight internal switch offsets individually retained'}
        total+=1;verdicts+=len(entry['verdicts'])
    report['cadence-diagnostic-owner']=rows
    assert (total,verdicts)==(13,49)
    from elftools.elf.elffile import ELFFile
    import struct
    with open(d.b.BLOB,'rb')as stream:
        elf=ELFFile(stream)
        observed=list(struct.unpack('<8I',elf.get_section_by_name('.rodata').data()[0x621c:0x623c]))
        assert observed==[0x7db86,0x7db31,0x7db86,0x7dd5b,0x7db31,0x7db31,0x7dd19,0x7dda9]
    refusals={}
    for label,path in [('original',d.b.BLOB),('selected',str(diagnostic/'Cadence/working-tone-late-descriptor/candidate.o'))]:
        result=subprocess.run(['python3',str(d.ROOT/'tools/toolchain/jumptable.py'),path,'cadence_create'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        assert result.returncode==1 and result.stdout.startswith('UNPROVED:'), result.stdout
        refusals[label]={'exit':result.returncode,'result':result.stdout.strip()}
    report['jump_table_scope']={'original_observed_destinations':observed,'bounded_prover_refusals':refusals,
                                'claim':'eight native edges and constructor cases; no complete static guard/ABI proof'}
    report['denominator']={'complete_TU_cells':total,'shared_body_verdicts':verdicts}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(total,'complete-TU cells /',verdicts,'shared verdicts; all bystanders unchanged, no exact gain/loss')
    print('Only declared table removal, six bank-pointer corrections and eight bounded code destinations change nontext')


if __name__=='__main__':main()
