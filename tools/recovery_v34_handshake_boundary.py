#!/usr/bin/env python3
"""Read-only native/current handshake call and emitted-local inventory.

Resolve both relocation-backed calls and ordinary local direct calls. Captured period cgraph is used only after its object raw-matches the frozen
dfc23b7d1 baseline. It does not describe later canonical snapshot revisions.
"""
import json,hashlib,re
from pathlib import Path
from collections import Counter
def parse_args():
    import argparse
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,default=root/'build/v34-handshake-boundary293')
    return parser.parse_args()

if __name__=='__main__':
    CLI_ARGS=parse_args()

import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86 import X86_OP_IMM
ROOT=d.ROOT;OUT=ROOT/'build/v34-handshake-boundary293';TARGET='v34handshak'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inspect(path,target_name=TARGET):
    with open(path,'rb') as stream:
        elf=ELFFile(stream);sections=list(elf.iter_sections());ti=elf.get_section_index('.text');data=sections[ti].data();table=elf.get_section_by_name('.symtab');symbols=list(table.iter_symbols());functions=[s for s in symbols if s['st_info']['type']=='STT_FUNC' and s['st_shndx']==ti and s['st_size']];entries={s['st_value']:s.name for s in functions};fn=next(s for s in functions if s.name==target_name);relocs={}
        for sec in sections:
            if sec['sh_type']!='SHT_REL' or sec['sh_info']!=ti:continue
            tab=elf.get_section(sec['sh_link'])
            for row in sec.iter_relocations():relocs[row['r_offset']]=(row['r_info_type'],tab.get_symbol(row['r_info_sym']))
        decoder=Cs(CS_ARCH_X86,CS_MODE_32);decoder.detail=True;instructions=list(decoder.disasm(data[fn['st_value']:fn['st_value']+fn['st_size']],fn['st_value']));calls=[];indirect=[]
        for ins in instructions:
            if ins.mnemonic not in ('call','jmp'):continue
            if ins.operands[0].type!=X86_OP_IMM:
                indirect.append({'at':ins.address,'op':ins.mnemonic,'operand':ins.op_str});continue
            target=ins.operands[0].imm;row=relocs.get(ins.address+1)
            if row:
                kind,symbol=row;assert kind==2
                if symbol.name and symbol['st_info']['type']!='STT_SECTION':name=symbol.name
                else:
                    add=int.from_bytes(data[ins.address+1:ins.address+5],'little',signed=True);target=symbol['st_value']+add+4;name=entries.get(target,'unresolved:'+hex(target))
            else:name=entries.get(target,'interior:'+hex(target))
            if ins.mnemonic=='call' or not name.startswith('interior:'):calls.append({'at':ins.address,'op':ins.mnemonic,'target':name})
        return {'size':fn['st_size'],'start':fn['st_value'],'decoded_instructions_including_padding':len(instructions),'calls':calls,'call_target_counts':dict(Counter(x['target'] for x in calls if x['op']=='call')),'indirect_transfers':indirect,'defined_text_functions':{s.name:{'size':s['st_size'],'binding':s['st_info']['bind']} for s in functions}}
def main():
    OUT.mkdir(exist_ok=True);current=ROOT/'build/snapshot-dfc-baseline/src_pump_v34_V34hshak.c.o';observed=ROOT/'build/v34-timing-visibility293/native-graph/V34hshak.c.o';assert current.read_bytes()==observed.read_bytes(),'old compiler trace no longer describes canonical object'
    trace=observed.parent/'V34hshak.c.00.cgraph';native=inspect(d.b.BLOB);candidate=inspect(current);extras={n:v for n,v in candidate['defined_text_functions'].items() if n not in native['defined_text_functions']};report={'native_object':str(d.b.BLOB),'native_sha256':sha(Path(d.b.BLOB)),'baseline_source_pin':'dfc23b7d1e92f5913b22b124771211d3e4650771','captured_baseline_object':str(current),'canonical_sha256':sha(current),'config':(ROOT/'build/snapshot-dfc-baseline/.build-config').read_text(),'compiler_trace_object':str(observed),'trace_object_raw_equal_canonical':True,'trace_sha256':sha(trace),'native':native,'current':candidate,'extra_functions':extras,'local_extra_count':sum(v['binding']=='STB_LOCAL' for v in extras.values()),'trnseg4a_inline_records':[line for line in trace.read_text().splitlines() if 'v34tx1_trnseg4a' in line and ('insns' in line or 'called by' in line)]}
    text=trace.read_text();a=text.index('Considering v34tx1_trnseg4a 877 insns.');report['trnseg4a_inline_decision']=text[a:text.index('\n\n',a)]
    assert native['call_target_counts'].get('v34tx1_trnseg4a',0)==0 and candidate['call_target_counts'].get('v34tx1_trnseg4a',0)==1
    (OUT/'inventory.json').write_text(json.dumps(report,indent=2)+'\n');print('native/current size',native['size'],candidate['size'],'calls',sum(native['call_target_counts'].values()),sum(candidate['call_target_counts'].values()),'extra locals',report['local_extra_count']);print('current local calls',{n:c for n,c in candidate['call_target_counts'].items() if n in extras});print(report['trnseg4a_inline_decision'])
if __name__=='__main__':
    OUT=CLI_ARGS.out.resolve()
    main()
