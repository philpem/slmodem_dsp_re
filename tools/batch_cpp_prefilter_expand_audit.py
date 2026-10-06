#!/usr/bin/env python3
"""Audit full EIA6 controls and confirm observed backedge removal."""
import hashlib,json,re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
import jumptable
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/batch-cpp-prefilter-expand'
TARGET='_ZN12V90PreFilter12setParamEia6Ev'

def bodyrows(path):
    with Path(path).open('rb') as stream:
        table=ELFFile(stream).get_section_by_name('.symtab')
        symbol=table.get_symbol_by_name(TARGET)[0]
        start,size=symbol['st_value'],symbol['st_size']
    return jumptable.instructions(str(path),'.text',start,start+size)

def backedges(rows):
    return [[address,mnemonic,operands] for address,raw,mnemonic,operands in rows
            if mnemonic.startswith('j') and mnemonic!='jmp'
            and re.match('^[0-9a-f]+ ',operands) and int(operands.split()[0],16)<address]

def main():
    results=json.loads((OUT/'results.json').read_text());cells=results['families']['V90PreFilter']['cells']
    family=OUT/'V90PreFilter';base=inspect(family/'baseline/candidate.o')
    assert (family/'baseline/candidate.o').read_bytes()==(ROOT/'build/production-before/src_pump_v90_V90PreFilter.cpp.o').read_bytes()
    report={}
    expected={'baseline':604,'rolled-unequal':598,'expanded-relational':802,'expanded-unequal':792}
    for label,cell in cells.items():
        path=family/label/'candidate.o';metadata=inspect(path)
        assert hashlib.sha256(path.read_bytes()).hexdigest()==cell['object_hash']
        assert hashlib.sha256((path.parent/'V90PreFilter.cpp').read_bytes()).hexdigest()==cell['source_hash']
        assert sorted(d.b.sizes(str(path)))==cell['functions']
        for name,expected_grade in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(path),name)))==expected_grade
        changed=sorted(name for name in cell['functions'] if d.b.body(str(path),name)!=d.b.body(str(family/'baseline/candidate.o'),name))
        assert changed==sorted(cell.get('changed_bodies',[]))
        assert len(cell['functions'])==16 and sum(v[0]=='EXACT' for v in cell['verdicts'].values())==6
        assert not cell.get('gains') and not cell.get('losses')
        assert cell.get('changed_bodies',[])==([] if label=='baseline' else [TARGET])
        for key in ('records','nobits','relocations'):assert metadata[key]==base[key],(label,key)
        changed_data={key:metadata['allocated'][key] for key in metadata['allocated'] if metadata['allocated'][key]!=base['allocated'][key]}
        if 'unequal' in label:
            assert changed_data=={'.rodata.cst4':base['allocated']['.rodata.cst4']+'00000000'}
        else:assert not changed_data
        rows=bodyrows(path);edges=backedges(rows)
        assert len(edges)==(0 if label.startswith('expanded') else 1)
        assert metadata['text_positions'][TARGET][1]==expected[label]
        report[label]={'target_bytes':expected[label],'conditional_backedges':edges,
                       'exact':[6,16],'changed_bodies':cell.get('changed_bodies',[]),
                       'changed_data':changed_data,'unchanged_bystanders':15}
    original=bodyrows(d.b.BLOB);assert len(backedges(original))==0
    output={'cells':4,'emitted_body_comparisons':64,'original_bytes':790,
            'original_conditional_backedges':0,'controls':report,'gains':[],'losses':[]}
    (OUT/'audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print('4 complete controls / 64 emitted bodies; 6/16 exact each; gains 0, losses 0')
    print('15 bystanders, bindings/imports/exports/BSS/nontext relocs unchanged in every control')
    print('Expansion detector fires: conditional backedges 1 -> 0; != adds only anonymous +0.0f pool entry')
if __name__=='__main__':main()
