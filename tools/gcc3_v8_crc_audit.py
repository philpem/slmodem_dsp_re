#!/usr/bin/env python3
"""Audit issue250's bounded controls and locate CRC extension transitions."""
import json
from pathlib import Path
import playbook_small_patterns as driver
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from gcc3_reload_trace import instructions

ROOT=driver.ROOT
b=driver.b
# Same bounded inspector used by the prior full-TU audits.
source=(ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
exec(source[source.index('def inspect'):source.index('reports=')])

def nodes(pattern):
    if isinstance(pattern,list):
        yield pattern
        for child in pattern[1:]:
            yield from nodes(child)

def stage(path):
    patterns=instructions(path.read_text())
    result={'instructions':len(patterns),'sign':[],'zero':[],'shift31':[],'shift15':[],'signed_memory':[],'unsigned_memory':[]}
    for uid,pattern in patterns.items():
        for node in nodes(pattern):
            if node[0] in ('sign_extend:SI','zero_extend:SI'):
                kind='sign' if node[0]=='sign_extend:SI' else 'zero'
                result[kind].append(uid)
                if isinstance(node[1],list) and node[1][0].startswith('mem'):
                    result['signed_memory' if kind=='sign' else 'unsigned_memory'].append(uid)
            if node[0]=='lshiftrt:SI' and isinstance(node[-1],list) and node[-1][:2] in (['const_int','31'],['const_int','15']):
                result['shift'+node[-1][1]].append(uid)
    return result

def main():
    roots=[ROOT/'build'/name for name in ('gcc3-v8-crc-rtl','gcc3-v8-crc-regmove','gcc3-v8-crc-order')]
    baseline=roots[0]/'full-plain/candidate.o'
    base=inspect(baseline)
    with baseline.open('rb') as stream:
        elf=ELFFile(stream)
        nontext={s.name:s.data() for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}
    report={'full_tu_cells':{},'stages':{},'refusals':{}}
    cells=fullcells=0
    source_hashes=set(); object_hashes=set()
    for root in roots:
        ledger=json.loads((root/'results.json').read_text())
        for label in ledger['cells']:
            folder=root/label;cells+=1
            source_hashes.add(ledger['cells'][label]['source_sha256'])
            object_hashes.add(ledger['cells'][label]['object_sha256'])
            if not label.startswith('extracted'):
                fullcells+=1
                metadata=inspect(folder/'candidate.o')
                for key in ('records','objects','allocated_sizes'):
                    assert metadata[key]==base[key],(root.name,label,key)
                with (folder/'candidate.o').open('rb') as stream:
                    elf=ELFFile(stream)
                    data={s.name:s.data() for s in elf.iter_sections() if s['sh_flags']&2 and s.name!='.text'}
                    assert data==nontext,(root.name,label,'nontext')
                    # Refuse silently masking relocated nontext; this TU's data have no relocations.
                    assert not any(s.name.startswith('.rel') and s['sh_info'] and elf.get_section(s['sh_info']).name!='.text' for s in elf.iter_sections()),'unexpected nontext relocation'
                report['full_tu_cells'][root.name+'/'+label]={'functions':len(b.sizes(str(folder/'candidate.o'))),'named_data':len(metadata['objects'])}
            for path in sorted(folder.glob('v8_crc.[0-9]*')):
                key=root.name+'/'+label+'/'+path.name
                try:
                    report['stages'][key]=stage(path)
                except ValueError as error:
                    report['refusals'][key]=str(error)
    assert all(key.endswith('.08.gcse') for key in report['refusals']), report['refusals']
    assert (cells,fullcells)==(13,11),(cells,fullcells)
    assert (len(source_hashes),len(object_hashes))==(4,6)
    report['distinct_sources']=len(source_hashes); report['distinct_objects']=len(object_hashes)
    def get(root,label,suffix):
        return report['stages'][root+'/'+label+'/v8_crc.'+suffix]
    for label in ('full-rtl','extracted-rtl'):
        before=get('gcc3-v8-crc-rtl',label,'18.cse2');after=get('gcc3-v8-crc-rtl',label,'20.combine')
        assert before['sign'] and before['shift31'] and not before['shift15']
        assert not after['sign'] and after['shift15'] and not after['shift31']
    for label in ('signed-field-on','signed-field-off'):
        before=get('gcc3-v8-crc-regmove',label,'21.ce2');after=get('gcc3-v8-crc-regmove',label,'22.regmove')
        assert before['sign'] and before['zero'] and not before['signed_memory']
        assert after['signed_memory'] and not after['unsigned_memory']
    recovered=get('gcc3-v8-crc-order','signed-first-rtl','22.regmove')
    assert recovered['unsigned_memory'] and recovered['sign'] and recovered['shift31'] and not recovered['signed_memory']
    baseobj=str(roots[2]/'full-rtl/candidate.o');candidate=str(roots[2]/'signed-first-rtl/candidate.o')
    changed=[n for n in b.sizes(baseobj) if b.body(baseobj,n)!=b.body(candidate,n)]
    assert changed==['v8_crc'],changed
    assert b.verdict(*b.body(b.BLOB,'v8_crc'),*b.body(candidate,'v8_crc'))==('EXACT',0)
    report['controls']={'attempted_cells':cells,'full_tu_cells':fullcells,'combine_controls':2,'regmove_signed_controls':2,'regmove_unsigned_control':1,'changed':changed}
    (ROOT/'build/v8-crc-rtl-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('13 cells; 11/11 full-TU metadata/data/raw nontext controls; 2/2 combine, 2/2 signed promotion and 1/1 unsigned promotion controls pass')
    print('stage observations:',len(report['stages']),'refusals:',len(report['refusals']),'full-TU functions:',len(b.sizes(str(baseline))),'named data:',len(base['objects']))
    print('source-order candidate EXACT, sole changed body v8_crc')

if __name__=='__main__':
    main()
