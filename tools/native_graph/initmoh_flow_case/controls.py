#!/usr/bin/env python3
"""Calibrate the scratch auditor on deliberately changed, real ELF inputs.

No repository source or legacy mutation machinery is used. Each control has a
declared semantic expectation before its ELF is emitted or analyzed.
"""
from pathlib import Path
import hashlib, io, json, sys, gc
from elftools.elf.elffile import ELFFile
import audit as a

HERE=Path(__file__).resolve().parent
CONTROLS=[
    ('baseline',None,None,None,True,'retained real ELF against native'),
    ('late_entry_dead_upper_sign',0x00f,'0fb64c2428','0fbe4c2428',True,'MOVSX changes only dead upper lanes; formal byte remains consumed'),
    ('late_reload_dead_upper_sign',0x062,'0fb644240b','0fbe44240b',True,'MOVSX upper 24 bits are dead after the shared reload'),
    ('role_dead_upper_zero',0x0fb,'0fbf939c350000','0fb7939c350000',True,'MOVZX versus MOVSX only changes dead upper 16 role bits at the void ABI'),
    ('late_test_widened',0x06d,'84c0','85c0',True,'32-bit TEST consumes zero-extended late, keeping the same live ZF'),
    ('wrong_late_stack_byte',0x062,'0fb644240b','0fb644240a',False,'neighbor stack byte does not carry late'),
    ('wrong_flag_input',0x039,'0fb64c242c','0fb64c2428',False,'independent flag is incorrectly replaced with late'),
    ('wrong_progress_owner',0x022,'8d7b04','8d7b08',False,'engine owner is displaced by four bytes'),
    ('wrong_tone_owner',0x0d8,'8dbd5c6f0000','8dbd606f0000',False,'reset receiver is displaced by four bytes'),
    ('wrong_call_argument',0x0b1,'891c24','892c24',False,'handshake receives session instead of object'),
    ('lost_live_callee_saved_zero',0x0e5,'31f6','31c9',False,'ESI no longer carries zero through tone reset into detector y1'),
    ('wrong_predicate_lane',0x06d,'84c0','84e4',False,'TEST AH observes a zero upper byte instead of late'),
    ('signed_debug_predicate',0x167,'7745','7f45',False,'signed JG differs from unsigned JA for high-bit debug words'),
    ('wrong_role_selector',0x11b,'6683fa65','6683fa67',False,'changed selector partitions the path domain incorrectly'),
    ('call_to_entry_plus_four',0x0b5,'fcffffff','00000000','refuse','PC32 same symbol with addend 0 calls entry+4 and must be refused'),
    ('debug_global_plus_four',0x162,'00000000','04000000','refuse','R_386_32 same global with addend 4 addresses its neighbor'),
    ('duplicate_relocation','duplicate-relocation',None,None,'refuse','two relocation records overlap one patched field'),
    ('overlapping_relocation','overlapping-relocation',None,None,'refuse','relocation fields overlap at different offsets'),
    ('alternate_entry_alias','symbol-alias',None,None,'refuse','a second externally visible function entry begins inside the audited body'),
    ('unknown_implicit_multiply',0x076,'31d2','f7e2','refuse','MUL has unsupported implicit EDX:EAX effects'),
    ('wrong_elf_architecture','header',None,None,'refuse','EM_X86_64 is outside the admitted decoder architecture'),
]
EXPECTED_REFUSALS={
    'call_to_entry_plus_four':('call_relocation','unsupported call relocation site/type/addend; requires PC32 function entry (-4)'),
    'debug_global_plus_four':('debug_relocation','unsupported debug global relocation site/type/addend; requires R_386_32 +0'),
    'duplicate_relocation':('relocation_overlap','duplicate or overlapping relocation fields'),
    'overlapping_relocation':('relocation_overlap','duplicate or overlapping relocation fields'),
    'alternate_entry_alias':('alternate_entry','unsupported alternate/incoming function entry aliases: '),
    'unknown_implicit_multiply':('unsupported_instruction','unsupported mul at 76'),
    'wrong_elf_architecture':('elf_architecture','requires little-endian ELF32 i386 relocatable input'),
}

def expected_refusal(name,exception):
    code,message=EXPECTED_REFUSALS[name]
    return isinstance(exception,a.AuditRefusal) and exception.code==code and (str(exception).startswith(message) if name=='alternate_entry_alias' else str(exception)==message)

def main():
    folder=HERE/('controls-optimized' if sys.flags.optimize else 'controls-normal');folder.mkdir(exist_ok=True)
    lifetime=a.Nodes();lifetime_failures=[]
    for k in range(512):
        value=a.z.BitVec('artifact_lifetime',32)+a.B(k);node=lifetime.add(value);right=lifetime.nodes[node]['args'][1]
        if lifetime.nodes[right].get('value')!=k:lifetime_failures.append(k)
        del value
        if k%16==0:gc.collect()
    lifetime_result={'denominator':512,'passed':512-len(lifetime_failures),'failed':len(lifetime_failures),'retained_expressions':len(lifetime.expressions),'node_count':len(lifetime.nodes)}
    print(json.dumps({'expression_lifetime_control':lifetime_result}),flush=True)
    refusal_checks=[]
    for name,(code,message) in EXPECTED_REFUSALS.items():
        refusal_checks.extend([expected_refusal(name,a.AuditRefusal(code,message)),not expected_refusal(name,RuntimeError(message)),not expected_refusal(name,a.AuditRefusal('wrong_code',message)),not expected_refusal(name,a.AuditRefusal(code,'unrelated admission failure'))])
    refusal_guard_result={'denominator':len(refusal_checks),'passed':sum(refusal_checks),'failed':sum(not r for r in refusal_checks)}
    print(json.dumps({'typed_refusal_guard_control':refusal_guard_result}),flush=True)
    raw=(HERE/'candidate.o').read_bytes();elf=ELFFile(io.BytesIO(raw));sym=next(s for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name==a.NAME)
    file_start=elf.get_section(sym['st_shndx'])['sh_offset']+sym['st_value']
    results=[]
    for name,offset,old,new,expected,explanation in CONTROLS:
        data=bytearray(raw)
        if offset=='header':data[18:20]=(62).to_bytes(2,'little')
        elif offset in ['duplicate-relocation','overlapping-relocation']:
            rs=next(s for s in elf.iter_sections() if s.name=='.rel.text');records=list(rs.iter_relocations());index=next(k for k,r in enumerate(records) if r['r_offset']==sym['st_value']+0xb5)
            # Keep section size and inventory fixed; replace the following
            # record with a duplicate or one-byte-overlapping record.
            src=rs['sh_offset']+index*rs['sh_entsize'];dst=src+rs['sh_entsize'];record=bytearray(data[src:src+rs['sh_entsize']])
            if offset=='overlapping-relocation':record[:4]=(records[index]['r_offset']+1).to_bytes(4,'little')
            data[dst:dst+rs['sh_entsize']]=record
        elif offset=='symbol-alias':
            table=elf.get_section_by_name('.symtab');alias_index=next(k for k,s in enumerate(table.iter_symbols()) if s.name!=a.NAME and s['st_shndx']==sym['st_shndx'] and s['st_info']['type']=='STT_FUNC')
            p=table['sh_offset']+alias_index*table['sh_entsize']+4;data[p:p+4]=(sym['st_value']+3).to_bytes(4,'little')
        elif offset is not None:
            before=bytes.fromhex(old);after=bytes.fromhex(new);a.need(len(before)==len(after),'control must preserve byte extent')
            p=file_start+offset;a.need(data[p:p+len(before)]==before,'wrong control bytes for '+name);data[p:p+len(before)]=after
        path=folder/(name+'.o');path.write_bytes(data)
        a.PROOFS.clear();a.NODES=a.Nodes()
        try:
            result=a.compare(a.elf_function(HERE/'native.o'),a.elf_function(path))
            got=result['equal'];failures=[p for p in a.PROOFS if p['result'] not in ['identical-expression','unsat']]
            row={'name':name,'expected':expected,'observed':got,'passed':got==expected,'explanation':explanation,'elf_sha256':hashlib.sha256(data).hexdigest(),'denominators':result['denominators'],'checks':len(a.PROOFS),'differences':len(failures),'first_difference':failures[0] if failures else None,'failed_path_domains':sum(not p['complete_path_domain_equal'] for p in result['paths'])}
        except Exception as exc:
            row={'name':name,'expected':expected,'observed':'refuse' if isinstance(exc,a.AuditRefusal) else 'apparatus-error','passed':expected=='refuse' and expected_refusal(name,exc),'explanation':explanation,'exception':type(exc).__name__+': '+str(exc),'refusal_code':getattr(exc,'code',None),'expected_refusal':EXPECTED_REFUSALS.get(name),'elf_sha256':hashlib.sha256(data).hexdigest()}
        results.append(row);print(json.dumps({k:v for k,v in row.items() if k in ['name','expected','observed','passed','checks','differences','failed_path_domains','exception']}),flush=True)
    out={'optimized':bool(sys.flags.optimize),'controls':results,'denominator':len(results),'passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results),'expression_lifetime_control':lifetime_result,'typed_refusal_guard_control':refusal_guard_result,'auditor_sha256':hashlib.sha256((HERE/'audit.py').read_bytes()).hexdigest(),'control_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'z3':a.z.get_version_string(),'capstone':a.cs.__version__,'feasibility_cache':a.FEASIBILITY_STATS}
    (folder/'results.json').write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({'controls':len(results),'passed':out['passed'],'failed':out['failed'],'optimized':out['optimized']}))
    return bool(out['failed'] or lifetime_result['failed'] or refusal_guard_result['failed'])

if __name__=='__main__':sys.exit(main())
