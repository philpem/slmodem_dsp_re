#!/usr/bin/env python3
"""Actual ELF adversarial controls and full census for readonly-copy identity.

Uses existing Gentoo canonical objects; performs no compilation or source edit.
The shared census engine retains all before/after body and verdict inventories.
"""
import json
from pathlib import Path
import struct
import sys
import playbook_small_patterns as d
import recovery_body_extraction as audit
import readonly_copy
from elftools.elf.elffile import ELFFile

NAME = audit.SYMBOL
REV = '51f62d213'


def controls(path,out,old):
    raw = path.read_bytes()
    proofs = {label:readonly_copy.prove(p,NAME) for label,p in [('reference',d.b.BLOB),('candidate',str(path))]}
    assert proofs['reference']['identity'] == proofs['candidate']['identity']
    assert old.verdict(*old.body(d.b.BLOB,NAME),*old.body(str(path),NAME)) == ('UNRESOLVED',1)
    assert d.b.verdict(*d.b.body(d.b.BLOB,NAME),*d.b.body(str(path),NAME)) == ('EXACT',0)
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        symbols = list(tab.iter_symbols())
        fn = next(s for s in symbols if s.name == NAME)
        section = elf.get_section(fn['st_shndx'])
        code, start = section['sh_offset']+fn['st_value'],fn['st_value']
        lit = elf.get_section_by_name(proofs['candidate']['table_section'])
        literal_index = elf.get_section_index(lit.name)
        table = lit['sh_offset']+proofs['candidate']['table_offset']
        flags = elf['e_shoff']+literal_index*elf['e_shentsize']+8
        code_symbol = next(i for i,s in enumerate(symbols) if s['st_info']['type']=='STT_SECTION' and s['st_shndx']==fn['st_shndx'])
        other = next(i for i,s in enumerate(symbols) if s['st_info']['type']=='STT_FUNC' and s.name!=NAME and s['st_shndx']==fn['st_shndx'])
        other_value = tab['sh_offset']+other*tab['sh_entsize']+4
        external = next((sec, i, r) for sec in elf.iter_sections() if sec['sh_type']=='SHT_REL' and sec['sh_info']==fn['st_shndx']
            for i,r in enumerate(sec.iter_relocations()) if not start<=r['r_offset']<start+fn['st_size'])
        relsec,relindex,rel = external
        relocation_info = relsec['sh_offset']+relindex*relsec['sh_entsize']+4
        relocation_operand = section['sh_offset']+rel['r_offset']
        metadata = dict(symtab=elf.get_section_index('.symtab'),literal_index=literal_index,
            section_offset=section['sh_offset'],literal_size=lit['sh_size'],literal_flags=lit['sh_flags'])
        object_indices = [i for i,s in enumerate(symbols) if s['st_info']['type']=='STT_OBJECT'][:2]
        assert len(object_indices) == 2
        object_entries = [tab['sh_offset']+i*tab['sh_entsize'] for i in object_indices]
    records = {}
    def cell(label,edit,unsafe=True):
        changed = bytearray(raw)
        edit(changed)
        target = out/(label+'.o')
        target.write_bytes(changed)
        body = d.b.body(str(target),NAME)
        same = d.b.verdict(*body,*body)
        versus = d.b.verdict(*d.b.body(d.b.BLOB,NAME),*body)
        try:
            proof = readonly_copy.prove(str(target),NAME)
            reason = None
        except readonly_copy.Refused as exc:
            proof,reason = None,str(exc)
        if unsafe:
            assert proof is None and same[0] in ('UNRESOLVED','NODATA'), (label,same,reason)
        else:
            assert proof is not None and same == ('EXACT',0), (label,same,reason)
            assert versus[0] != 'EXACT', (label,versus)
        records[label] = {'object_sha256':audit.digest(target),'unsafe':unsafe,
            'actual_same_pair_verdict':same,'actual_vs_reference':versus,'refusal':reason,
            'proved_count':proof['count'] if proof else None}
    def put(raw,at,data):
        raw[at:at+len(data)] = data
    # Content changes are safe identity instances; reject against the real
    # original, while accepting their identical pair. Ditto a shorter copy.
    cell('changed-last-table-byte',lambda r:r.__setitem__(table+63,r[table+63]^1),False)
    cell('safe-count-eight',lambda r:r.__setitem__(code+15,8),False)
    cell('short-literal-extent',lambda r:put(r,code+40,(metadata['literal_size']-60).to_bytes(4,'little')))
    cell('source-register-not-killed',lambda r:put(r,code+52,bytes.fromhex('89f090')))
    cell('source-address-escapes',lambda r:put(r,code+52,bytes.fromhex('893390')))
    cell('source-pointer-copied-before-rep',lambda r:put(r,code+44,bytes.fromhex('89f39090')))
    cell('source-pointer-compared-before-rep',lambda r:put(r,code+48,bytes.fromhex('39f6')))
    cell('partial-source-kill',lambda r:put(r,code+52,bytes.fromhex('6631f6')))
    cell('direction-flag-set',lambda r:r.__setitem__(code,0xfd))
    cell('direction-flag-unproved',lambda r:r.__setitem__(code,0x90))
    cell('unknown-copy-count',lambda r:put(r,code+14,bytes.fromhex('89d1909090')))
    cell('too-large-copy-count',lambda r:r.__setitem__(code+15,17))
    cell('writable-source',lambda r:put(r,flags,(metadata['literal_flags']|1).to_bytes(4,'little')))
    cell('unallocated-source',lambda r:put(r,flags,bytes(4)))
    def ambiguous(r):
        for entry in object_entries:
            put(r,entry+4,struct.pack('<II',0,64))
            put(r,entry+14,metadata['literal_index'].to_bytes(2,'little'))
    cell('ambiguous-literal-owners',ambiguous)
    cell('destination-not-private-frame',lambda r:put(r,code+33,bytes.fromhex('89ef')))
    cell('copy-overwrites-saved-registers',lambda r:r.__setitem__(code+15,20))
    cell('branch-back-to-rep',lambda r:r.__setitem__(code+92,0xd5))
    cell('external-branch-into-copy',lambda r:put(r,metadata['section_offset'],b'\xe9'+(start+0x32-5).to_bytes(4,'little',signed=True)))
    def interior(r):
        put(r,relocation_info,((code_symbol<<8)|1).to_bytes(4,'little'))
        put(r,relocation_operand,(start+0x32).to_bytes(4,'little'))
    cell('relocated-interior-address',interior)
    for label,offset in [('relocation-crosses-function-start',start-1),
                         ('relocation-crosses-function-end',start+fn['st_size']-1)]:
        cell(label,lambda r,offset=offset:put(r,relocation_info-4,offset.to_bytes(4,'little')))
    cell('named-overlap-entry',lambda r:put(r,other_value,(start+0x32).to_bytes(4,'little')))
    def data_relocation(r,kind=1,link=None,offset=0,rela=False):
        old_shoff = int.from_bytes(r[32:36],'little')
        count = int.from_bytes(r[48:50],'little')
        headers = bytes(r[old_shoff:old_shoff+40*count])
        payload = len(r)
        r.extend(struct.pack('<II',offset,(code_symbol<<8)|kind))
        if rela:
            r.extend(bytes(4))
        shoff = len(r)
        r.extend(headers)
        r.extend(struct.pack('<10I',0,4 if rela else 9,0,0,payload,12 if rela else 8,
            metadata['symtab'] if link is None else link,metadata['literal_index'],4,12 if rela else 8))
        put(r,32,shoff.to_bytes(4,'little'))
        put(r,48,(count+1).to_bytes(2,'little'))
    cell('relocated-literal-bytes',data_relocation)
    cell('unknown-literal-relocation-width',lambda r:data_relocation(r,3))
    cell('wrong-relocation-symbol-table',lambda r:data_relocation(r,link=metadata['literal_index']))
    cell('unsupported-rela-semantics',lambda r:data_relocation(r,rela=True))
    # Actual proof metadata contains bytes by design; JSON preserves their hex.
    (out/'proofs.json').write_text(json.dumps(proofs,indent=2,default=lambda v:v.hex() if isinstance(v,bytes) else str(v))+'\n')
    print('readonly-copy controls:',len(records),'actual ELF cells;',sum(v['unsafe'] for v in records.values()),
        'hazardous/unsupported same-pairs refused; two safe variants compared against original',flush=True)
    return records


def validate(report):
    expected = [{'symbol':NAME,'before':('UNRESOLVED',1),'after':('EXACT',0)}]
    assert report['verdict_changes'] == expected, report['verdict_changes']
    assert sorted(report['body_changes']) == sorted([
        ['reference',NAME,True,True],[audit.OBJECT,NAME,True,True]]), report['body_changes']
    assert report['shared_symbols'] == 1852 and report['candidate_body_copies'] == 1886


if __name__ == '__main__':
    audit.BASE = REV
    audit.PROVENANCE_PATHS = audit.PROVENANCE_PATHS + (Path(__file__),)
    audit.IDENTITY_SCOPE = 'all'
    audit.mutations = controls
    audit.validate_census = validate
    if '--out' not in sys.argv:
        sys.argv.extend(['--out',str(d.ROOT/'build/readonly-copy')])
    audit.main()
