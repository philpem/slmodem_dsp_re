#!/usr/bin/env python3
"""Audit literal consumers beyond mergeable strings without typing numeric tables.

Only mergeable literals and ordinary addresses tracked to a diagnostic format
slot are strong candidates. Printable scalar/indexed loads remain ambiguous.
Counts describe static relocation sites, never path frequency or reachability.
"""
import argparse
from collections import Counter, defaultdict
from difflib import SequenceMatcher
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import bisect
_saved = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
from elftools.elf.elffile import ELFFile
sys.path[:] = _saved
import recovery_large_debug_audit as calls

ROOT = Path(__file__).resolve().parents[1]
DEBUG = {'dsplibs_debug_printf', 'edprintf', 'printf', 'puts'}


def text_at(raw, start, minimum=1):
    if not 0 <= start < len(raw):
        return None
    end = raw.find(b'\0', start, min(len(raw), start + 4097))
    if end < 0:
        return None
    data = raw[start:end]
    if len(data) < minimum or any(c not in (9, 10, 13) and not 32 <= c < 127 for c in data):
        return None
    return data.decode('ascii')


def decode(path):
    """Keep instruction and relocation boundaries from actual object disassembly."""
    text = subprocess.check_output(['objdump', '-dr', str(path)], text=True)
    rows, current, section = [], None, None
    for line in text.splitlines():
        match = re.match(r'Disassembly of section (.*):', line)
        if match:
            section = match[1]
        match = re.match(r'^\s*([0-9a-f]+):\s+R_386_(\S+)\s+(\S+)', line)
        if match and current is not None:
            current['relocs'].append((int(match[1],16), match[2], match[3]))
            continue
        match = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*([a-z][a-z0-9]*)\s*(.*)', line)
        if match:
            current = {'section':section, 'at':int(match[1],16), 'op':match[3], 'args':match[4], 'size':len(match[2].split()), 'relocs':[]}
            rows.append(current)
    return rows, text


def external_storage(paths):
    """Unique global OBJECT definitions only; duplicates stay unresolved."""
    found = defaultdict(list)
    for path in paths:
        with path.open('rb') as f:
            elf = ELFFile(f)
            for sym in elf.get_section_by_name('.symtab').iter_symbols():
                if sym['st_info']['type'] != 'STT_OBJECT' or sym['st_info']['bind'] == 'STB_LOCAL' or not isinstance(sym['st_shndx'],int):
                    continue
                sec = elf.get_section(sym['st_shndx'])
                if sec['sh_type']=='SHT_NOBITS': continue
                found[sym.name].append({'object':str(path),'section':sec.name,'raw':sec.data(),
                    'file_offset':sec['sh_offset'],'flags':sec['sh_flags'],'value':sym['st_value'],
                    'size':sym['st_size'],'binding':sym['st_info']['bind']})
    return {n:rows[0] for n,rows in found.items() if len(rows)==1}


def inventory(path, external=None):
    instructions, assembly = decode(path)
    operand = {(i['section'],at):i for i in instructions for at,kind,target in i['relocs']}
    result, records, functions = defaultdict(list), [], {}
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        owners, storage = defaultdict(list), defaultdict(list)
        file_owner = ''
        for sym in tab.iter_symbols():
            if sym['st_info']['type'] == 'STT_FILE':
                file_owner = sym.name
            if not isinstance(sym['st_shndx'],int):
                continue
            if sym['st_info']['type'] == 'STT_FUNC':
                owners[sym['st_shndx']].append(sym)
                functions[sym.name] = (elf.get_section(sym['st_shndx']).name, sym['st_value'], sym['st_size'])
                result[sym.name]
            if sym['st_info']['type'] == 'STT_OBJECT':
                storage[sym['st_shndx']].append((sym,file_owner))
        for relsec in elf.iter_sections():
            if relsec['sh_type'] != 'SHT_REL' or relsec['sh_info'] not in owners:
                continue
            code_section = elf.get_section(relsec['sh_info'])
            code = code_section.data()
            names = elf.get_section(relsec['sh_link'])
            for reloc in relsec.iter_relocations():
                if reloc['r_info_type'] != 1:
                    continue
                target = names.get_symbol(reloc['r_info_sym'])
                off = reloc['r_offset']
                addend = int.from_bytes(code[off:off+4], 'little', signed=True)
                provider = external.get(target.name) if external and target['st_shndx']=='SHN_UNDEF' else None
                if provider:
                    section_name,raw = provider['section'],provider['raw']
                    start=provider['value']+addend
                    data_file_offset=provider['file_offset'];data_flags=provider['flags']
                    named=[{'name':target.name,'local_FILE':None,'offset':addend,
                            'size':provider['size'],'binding':provider['binding']}]
                else:
                    if not isinstance(target['st_shndx'],int): continue
                    data=elf.get_section(target['st_shndx'])
                    if data['sh_type']=='SHT_NOBITS': continue
                    section_name,raw=data.name,data.data()
                    start=target['st_value']+addend
                    data_file_offset=data['sh_offset'];data_flags=data['sh_flags']
                    named=[{'name':s.name,'local_FILE':f if s['st_info']['bind']=='STB_LOCAL' else None,'offset':start-s['st_value'],'size':s['st_size'],'binding':s['st_info']['bind']}
                           for s,f in storage[target['st_shndx']] if s['st_value']<=start<s['st_value']+s['st_size']]
                if not (section_name.startswith('.rodata') or section_name.startswith('.data')): continue
                literal=text_at(raw,start,0 if section_name.startswith('.rodata.str') else 4)
                if literal is None: continue
                inst=operand.get((code_section.name,off),{})
                addressed=((inst.get('op','').startswith('mov') or inst.get('op')=='push') and inst.get('args','').startswith('$') and off==inst.get('at',-99)+inst.get('size',0)-4) or (inst.get('op','').startswith('lea') and '%' not in inst.get('args','').split(',')[0])
                windows=[]
                for owner_info in named:
                    begin=start-owner_info['offset']
                    owner_bytes=raw[begin:begin+owner_info['size']]
                    for match in re.finditer(rb'[\x20-\x7e\t\r\n]{4,}\x00',owner_bytes):
                        windows.append({'owner':owner_info['name'],'offset':match.start(),
                                        'text':match[0][:-1].decode('ascii')})
                kind = 'mergeable_literal' if section_name.startswith('.rodata.str') else 'ordinary_address_candidate' if addressed else 'ambiguous_printable_load'
                for owner in owners[relsec['sh_info']]:
                    if not owner['st_value'] <= off < owner['st_value'] + owner['st_size']:
                        continue
                    row = {'function':owner.name,'consumer_offset':off-owner['st_value'],'relocation_file_offset':code_section['sh_offset']+off,'instruction':inst,
                           'relocation':{'type':'R_386_32','symbol':target.name,'addend':addend},
                           'storage':{'section':section_name,'offset':start,'file_offset':data_file_offset+start,'flags':data_flags,'owners':named,'object':provider['object'] if provider else str(path),'crosses_named_owner':any(o['offset']+len(literal)+1>o['size'] for o in named)},
                           'text':literal,'named_storage_printable_windows':windows,'classification':kind,'strong':kind == 'mergeable_literal'}
                    result[owner.name].append(row)
                    records.append(row)
    # A deliberately bounded address trace within one straight-line block.
    # Unknown writes, calls and control transfers discard tracked provenance.
    by_section = defaultdict(list)
    for inst in instructions:
        by_section[inst['section']].append(inst)
    section_offsets = {s:[i['at'] for i in seq] for s,seq in by_section.items()}
    branch_entries=defaultdict(set)
    for inst in instructions:
        if inst['op'].startswith('j'):
            target=re.match(r'([0-9a-f]+) <',inst['args'])
            if target: branch_entries[inst['section']].add(int(target[1],16))
    for name, (section, lo, size) in functions.items():
        refs = {r['instruction'].get('at'):r for r in result[name] if r['classification'] in ('ordinary_address_candidate','mergeable_literal')}
        if not refs:
            continue
        regs, slots = {}, {}
        offsets=section_offsets[section]
        for inst in by_section[section][bisect.bisect_left(offsets,lo):bisect.bisect_left(offsets,lo+size)]:
            if inst['at'] in branch_entries[section]: regs,slots={},{}
            op, args = inst['op'],inst['args'].split(' #')[0]
            if op.startswith('mov') or op.startswith('lea'):
                parts = args.rsplit(',',1)
                if len(parts)==2:
                    src,dst = map(str.strip,parts)
                    row = refs.get(inst['at'])
                    origin = row if row else regs.get(src)
                    if dst.startswith('%'):
                        if origin: regs[dst]=origin
                        else: regs.pop(dst,None)
                    elif '%esp' in dst:
                        if origin: slots[dst]=origin
                        else: slots.pop(dst,None)
            elif op in ('call','jmp'):
                targets = [t.split('-')[0] for at,k,t in inst['relocs'] if k=='PC32']
                row=slots.get('(%esp)') or slots.get('0x0(%esp)')
                if row:
                    row['consumer_call']={'at':inst['at']-lo,'targets':targets,'slot':'stack+0'}
                    if any(t in DEBUG for t in targets) and row['classification']=='ordinary_address_candidate':
                        row['classification']='ordinary_diagnostic_format'
                        row['strong']=True
                        row['diagnostic_call']=row['consumer_call']
                regs,slots={},{}
            elif op.startswith('j') or op in ('ret','leave','push','pop'):
                regs,slots={},{}
            elif ',' in args:
                if args.rsplit(',',1)[1].strip()=='%esp': slots={}
                regs.pop(args.rsplit(',',1)[1].strip(),None)
    return dict(result), records


def values(rows, strong=True):
    return {r['text'] for r in rows if not strong or r['strong']}


def differences(original, candidate):
    a,b=values(original),values(candidate)
    missing,extra=sorted(a-b),sorted(b-a)
    substitutions=[]
    for left in missing:
        scored=[(SequenceMatcher(None,left,right).ratio(),right) for right in extra]
        if scored:
            ratio,right=max(scored)
            if ratio>=0.45:
                substitutions.append({'original':left,'candidate':right,'similarity':ratio,
                    'relation':'sliding-substring' if left in right or right in left else 'aligned-format-edit'})
    return {'missing':missing,'extra':extra,'possible_substitutions':substitutions}


def controls(reference, old_objects, output):
    # ELF fixtures are measurement apparatus, assembled by the host assembler.
    # A printable integer and an actual diagnostic format share the same data
    # spelling, so printable bytes alone cannot establish string semantics.
    assembly='''.text
.globl storage_probe
.type storage_probe,@function
storage_probe:
 subl $8,%esp
 movl $ordinary_format,%eax
 movl %eax,(%esp)
 call dsplibs_debug_printf
 movl $readonly_format,(%esp)
 call dsplibs_debug_printf
 movl numeric_words,%eax
 addl $8,%esp
 ret
.size storage_probe,.-storage_probe
.data
.type ordinary_format,@object
ordinary_format: .asciz "ordinary format %d\\n"
.size ordinary_format,.-ordinary_format
.section .rodata
.type readonly_format,@object
readonly_format: .asciz "readonly format %x\\n"
.size readonly_format,.-readonly_format
.type numeric_words,@object
numeric_words: .long 0x44434241,0
.size numeric_words,.-numeric_words
'''
    source=output/'storage-control.s';source.write_text(assembly)
    fixture=output/'storage-control.o'
    command=['as','--32','-o',str(fixture),str(source)]
    subprocess.run(command,check=True)
    base,rows=inventory(fixture)
    strong=[r for r in rows if r['strong']]
    assert len(strong)==2 and all(r['classification']=='ordinary_diagnostic_format' for r in strong)
    assert {r['storage']['section'] for r in strong}=={'.data','.rodata'}
    assert any(r['classification']=='ambiguous_printable_load' and r['text']=='ABCD' for r in rows)
    raw=bytearray(fixture.read_bytes());raw[strong[0]['storage']['file_offset']+strong[0]['text'].index('%')+1] = ord('x')
    wrong=output/'storage-control-wrong.o';wrong.write_bytes(raw)
    mutated,_=inventory(wrong)
    delta=differences(base['storage_probe'],mutated['storage_probe'])
    assert delta['missing'] and delta['extra'] and delta['possible_substitutions']
    assert len(raw)==fixture.stat().st_size
    assert len(base['storage_probe'])==len(mutated['storage_probe'])
    slid=bytearray(fixture.read_bytes())
    at=strong[0]['relocation_file_offset']
    value=int.from_bytes(slid[at:at+4],'little')
    slid[at:at+4]=(value+1).to_bytes(4,'little')
    slide=output/'storage-control-sliding.o';slide.write_bytes(slid)
    slid_inventory,_=inventory(slide)
    slide_delta=differences(base['storage_probe'],slid_inventory['storage_probe'])
    assert any(r['relation']=='sliding-substring' for r in slide_delta['possible_substitutions'])
    assert [(i['op'],i['args']) for i in decode(fixture)[0] if i['op']=='call']==[(i['op'],i['args']) for i in decode(wrong)[0] if i['op']=='call']
    known=[]
    for obj,name,needles in [
        ('src_pump_v90_VpcmFloModem.cpp.o','_ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_',('Echo Silence ON','Echo Silence Off')),
        ('src_pump_v90_V90Phase3Demodulator.cpp.o','_ZN20V90Phase3Demodulator14getV92DecisionEf',('enter Wait For V92Jd',))]:
        path=old_objects/obj
        old,_=inventory(path)
        d=differences(reference[name],old[name])
        assert all(any(needle in s for s in d['missing']) for needle in needles),d
        known.append({'object':str(path),'symbol':name,'detected':d})
    return {'host_assembler_command':command,'host_assembler_identity':subprocess.check_output(['as','--version'],text=True).splitlines()[0],
            'ordinary_format_same_length_ELF_mutation':delta,'relocation_addend_sliding_ELF_mutation':slide_delta,'printable_numeric_load_kept_ambiguous':True,'known_old_defects':known}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects',type=Path,required=True)
    ap.add_argument('--reference',type=Path,default=ROOT/'ref/slmodemd/dsplibs.o')
    ap.add_argument('--old-controls',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    reference,refrows=inventory(args.reference)
    evidence=args.out/'consumers';evidence.mkdir(exist_ok=True)
    (evidence/'reference.json').write_text(json.dumps(reference,indent=2)+'\n')
    report={'reference_sha256':hashlib.sha256(args.reference.read_bytes()).hexdigest(),
            'consumer_evidence_dir':str(evidence),'build_config':(args.objects/'.build-config').read_text(),'object_hashes':{},
            'objects':0,'shared_functions':0,'reference_consumer_sites':0,'candidate_consumer_sites':0,'reference_named_window_sites':0,'candidate_named_window_sites':0,
            'strong_rows':[],'ambiguous_rows':[],'named_storage_window_rows':[],'count_only_rows':[],'reference_classifications':Counter(),'candidate_classifications':Counter(),'same_TU_closure_explanations':[]}
    report['controls']=controls(reference,args.old_controls,args.out)
    paths=sorted(args.objects.glob('*.o'))
    providers=external_storage(paths)
    report['unique_external_storage_definitions']=len(providers)
    reference_edges=None
    for path in paths:
        report['objects']+=1;report['object_hashes'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        candidate,rows=inventory(path,providers);edges=None
        (evidence/(path.name+'.json')).write_text(json.dumps(candidate,indent=2)+'\n')
        for name in sorted(reference.keys() & candidate.keys()):
            report['shared_functions']+=1
            a,b=reference[name],candidate[name]
            report['reference_consumer_sites']+=len(a);report['candidate_consumer_sites']+=len(b)
            report['reference_classifications'].update(r['classification'] for r in a)
            report['candidate_classifications'].update(r['classification'] for r in b)
            report['reference_named_window_sites']+=sum(len(r['named_storage_printable_windows']) for r in a)
            report['candidate_named_window_sites']+=sum(len(r['named_storage_printable_windows']) for r in b)
            delta=differences(a,b)
            row={'object':path.name,'function':name,**delta,'original_consumers':a,'candidate_consumers':b}
            if delta['missing'] or delta['extra']:
                if edges is None:edges=calls.graph(path,True)
                reached=calls.closure(edges,name)
                expanded=[r for n in reached for r in candidate.get(n,[])]
                row['candidate_same_TU_closure_missing']=sorted(values(a)-values(expanded))
                if reference_edges is None: reference_edges=calls.graph(args.reference,True)
                restricted={n:{v for v in reference_edges.get(n,[]) if v in candidate} for n in candidate}
                original_reached=calls.closure(restricted,name)
                original_expanded=[r for n in original_reached for r in reference.get(n,[])]
                row['extra_unexplained_by_original_same_TU_closure']=sorted(values(b)-values(original_expanded))
                if delta['extra']==[''] and not delta['missing']:
                    empty=[r for r in b if r['text']=='']
                    if empty and all(r.get('consumer_call',{}).get('targets')==['nanf'] for r in empty):
                        row['review']='Empty tag passed to nanf at an observed stack+0 call; not an additional diagnostic.'
                if not row['candidate_same_TU_closure_missing'] and not row['extra_unexplained_by_original_same_TU_closure']:
                    report['same_TU_closure_explanations'].append(row)
                else:report['strong_rows'].append(row)
            elif Counter(r['text'] for r in a if r['strong']) != Counter(r['text'] for r in b if r['strong']):
                ac=Counter(r['text'] for r in a if r['strong'])
                bc=Counter(r['text'] for r in b if r['strong'])
                row['original_static_site_surplus']=dict(ac-bc)
                row['candidate_static_site_surplus']=dict(bc-ac)
                report['count_only_rows'].append(row)
            aw={(w['owner'],w['offset'],w['text']) for r in a for w in r['named_storage_printable_windows']}
            bw={(w['owner'],w['offset'],w['text']) for r in b for w in r['named_storage_printable_windows']}
            if aw != bw:
                report['named_storage_window_rows'].append({'object':path.name,'function':name,
                    'missing_aligned_windows':sorted(aw-bw),'extra_aligned_windows':sorted(bw-aw),
                    'missing_sliding_contents':sorted({v[2] for v in aw}-{v[2] for v in bw}),
                    'extra_sliding_contents':sorted({v[2] for v in bw}-{v[2] for v in aw}),
                    'status':'NUL-terminated printable windows within named object bounds; semantics remain unproven.'})
            ambiguous_a=[r for r in a if not r['strong']]
            ambiguous_b=[r for r in b if not r['strong']]
            if values(ambiguous_a,False)!=values(ambiguous_b,False):
                report['ambiguous_rows'].append({'object':path.name,'function':name,'original':ambiguous_a,'candidate':ambiguous_b,
                    'status':'Printable ordinary storage is not established as a string; operand/storage review required.'})
    assert report['objects'] and report['shared_functions']
    report['limits']='Static direct R_386_32 consumers only; no path/guard/multiplicity equivalence. Ordinary addresses become strong only when a bounded straight-line move trace reaches diagnostic stack+0. This syntactic trace is not an alias/path proof. Scalar/indexed printable data remain ambiguous. Named undefined OBJECT references resolve only against unique global definitions in the supplied object set. Indirect pointer tables, synthesized addresses, other calling conventions and string contents changed before use are outside coverage. Sliding substitutions nominate review and never excuse a missing byte. Same-TU closure is one-sided evidence of moved missing consumers; extra consumers and call-site count differences require review.'
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    summary={k:v for k,v in report.items() if k in ('objects','shared_functions','reference_consumer_sites','candidate_consumer_sites','reference_classifications','candidate_classifications','reference_named_window_sites','candidate_named_window_sites')}
    summary.update({k:len(report[k]) for k in ('strong_rows','ambiguous_rows','named_storage_window_rows','count_only_rows','same_TU_closure_explanations')})
    print(json.dumps(summary,indent=2),flush=True)
    for row in report['strong_rows']:
        print(row['object'],row['function'],repr(row['missing']),repr(row['extra']),flush=True)


if __name__=='__main__':main()
