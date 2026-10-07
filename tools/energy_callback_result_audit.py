#!/usr/bin/env python3
"""Audit finite energy-validator controls, value age and callee boundaries."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect,function_chunk
from gcc3_reload_trace import instructions
from gcc_x87_transfer_screen import assembly
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/energy-callback-result';TARGET='bValidateEnergyValue'

def calls(path):
    with Path(path).open('rb') as stream:
        elf=ELFFile(stream);tab=elf.get_section_by_name('.symtab');symbol=tab.get_symbol_by_name(TARGET)[0]
        start=symbol['st_value'];end=start+symbol['st_size'];targets=[]
        for section in elf.iter_sections():
            if not isinstance(section,RelocationSection) or section['sh_info']!=symbol['st_shndx']:continue
            for reloc in section.iter_relocations():
                if start<=reloc['r_offset']<end and reloc['r_info_type']==2:targets.append(tab.get_symbol(reloc['r_info_sym']).name)
        return targets

def main():
    result=json.loads((OUT/'results.json').read_text());cells=result['families']['Fdspkrnl']['cells']
    basepath=OUT/'Fdspkrnl/baseline/candidate.o';base=inspect(basepath);report={}
    for label,cell in cells.items():
        path=OUT/'Fdspkrnl'/label/'candidate.o';metadata=inspect(path)
        assert len(cell['functions'])==6 and sum(v[0]=='EXACT' for v in cell['verdicts'].values())==2
        assert not cell.get('gains') and not cell.get('losses')
        for key in ('records','allocated','nobits','relocations'):assert metadata[key]==base[key],(label,key)
        rows=instructions(function_chunk(path.parent/'Fdspkrnl.c.01.rtl',TARGET))
        call=next(u for u,p in rows.items() if 'fComputeRMSValueFloatBuf' in str(p))
        loads=[u for u,p in rows.items() if isinstance(p,list) and p[0]=='set' and 'mem:SI' in str(p[2]) and 'idxp' in str(p[2])]
        assert loads and ((list(rows).index(loads[0])>list(rows).index(call))==('sequenced-rms' in label))
        finalcalls=calls(path);assert finalcalls.count('fComputeRMSValueFloatBuf')==1
        assert 'FDSP_Kernel_InitObj' not in finalcalls
        asm=assembly(path)[TARGET]
        report[label]={'grade':cell['verdicts'][TARGET],'target_bytes':d.b.sizes(str(path))[TARGET],
                       'changed_bodies':cell.get('changed_bodies',[]),'initial_rms_call_uid':call,
                       'initial_index_load_uids':loads,'final_direct_call_targets':finalcalls,
                       'RET_count':sum(row[1]=='ret' for row in asm)}
    with Path(d.b.BLOB).open('rb') as stream:
        original_symbol=ELFFile(stream).get_section_by_name('.symtab').get_symbol_by_name(TARGET)[0]
        original_binding=original_symbol['st_info']['bind']
    assert original_binding=='STB_LOCAL'
    assert all(inspect(OUT/'Fdspkrnl'/label/'candidate.o')['records'][TARGET][1]=='STB_LOCAL' for label in cells)
    original=calls(d.b.BLOB);assert original.count('FDSP_Kernel_InitObj')==1
    (OUT/'audit.json').write_text(json.dumps({'cells':4,'emitted_body_comparisons':24,'raw_baseline_reproduced':cells['baseline']['baseline_reproduced'],
                                           'ABI':{'original_binding':original_binding,'retained_binding':'STB_LOCAL','observed_argument_carriers':['EAX:buf','EDX:n','stack+4:hist','stack+8:idxp','stack+12:histlen','stack+16:k'],'explicit_attribute_vs_optimizer_origin':'not_recovered'},'metadata_data_bss_nontext_equal':True,'original_call_targets':original,'controls':report,'gains':[],'losses':[]},indent=2)+'\n')
    print('4 full-TU cells /24 bodies;2/6exact each;0gains/losses;allmetadata/data/BSS/nontext unchanged')
    print('Positive callback detector: first index read moves after RMS in initial RTL only for sequenced controls')
    print('Original has1FDSP_Kernel_InitObj call; all4 controls inline it; no callee coercion')
if __name__=='__main__':main()
