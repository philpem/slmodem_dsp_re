#!/usr/bin/env python3
"""Read-only whole-object triage for identical non-stack instruction streams.

Stack normalization is diagnostic only, never a byte-identity comparator.
Every selected candidate still needs original operands and a callee witness.
"""
import argparse
import bisect
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

LINE = re.compile(r'^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*([^\s]+)(?:\s+(.*))?$')
def erase_colours(stream):
    return [[mn,re.sub(r'%e(?:ax|bx|cx|dx|si|di|bp)', '%R32',
                    re.sub(r'%(?:ax|bx|cx|dx|si|di|bp)\b','%R16',
                    re.sub(r'%(?:al|bl|cl|dl)\b','%R8L',
                    re.sub(r'%(?:ah|bh|ch|dh)\b','%R8H',ops))))] for mn,ops in stream]

MEMORY = re.compile(r'(?<![\w@])(?:-?0x[0-9a-f]+)?\(%esp\)')

def inventory(path):
    rows = {}; section = None
    for line in subprocess.check_output(['objdump','-dr',str(path)], text=True).splitlines():
        header = re.match(r'Disassembly of section (.+):', line)
        if header:
            section = header[1]
            continue
        m = LINE.match(line)
        if not m:
            continue
        at, raw, mnemonic, operands = m.groups()
        if mnemonic.startswith('R_386_') or re.fullmatch(r'[0-9a-f]{2}',mnemonic):
            continue
        operands = re.sub(r'<[^>]*>', '', (operands or '').split('#',1)[0]).strip()
        rows.setdefault(section, []).append([int(at,16),len(raw.split()),mnemonic,operands])
    with path.open('rb') as stream:
        elf = ELFFile(stream); tab = elf.get_section_by_name('.symtab')
        relocations = {}
        for sec in elf.iter_sections():
            if not isinstance(sec, RelocationSection): continue
            owner = elf.get_section(sec['sh_info'])
            if not owner['sh_flags'] & 4: continue
            for rel in sec.iter_relocations():
                target = tab.get_symbol(rel['r_info_sym'])
                name = target.name or elf.get_section(target['st_shndx']).name
                kind = {1:'R_386_32',2:'R_386_PC32'}[rel['r_info_type']]
                off = rel['r_offset']
                identity = d.b.relocation_target(kind,name,owner.data()[off:off+4],d.b.section_symbols(str(path)))
                relocations[(owner.name,off)] = (kind,identity)
        addresses_by_section = {name:[r[0] for r in records] for name,records in rows.items()}
        result = {}
        for sym in tab.iter_symbols():
            if sym['st_info']['type'] != 'STT_FUNC' or not isinstance(sym['st_shndx'],int) or not sym['st_size']: continue
            section = elf.get_section(sym['st_shndx']).name
            lo, hi = sym['st_value'], sym['st_value']+sym['st_size']
            starts = addresses_by_section.get(section,[])
            candidates = rows.get(section,[])[bisect.bisect_left(starts,lo):bisect.bisect_left(starts,hi)]
            fn = [r for r in candidates if not d.b._padding(r[2],r[3])]
            addresses = {r[0]:i for i,r in enumerate(fn)}
            normalized = []; original = []; frames = []; calls = []
            for at,size,mn,ops in fn:
                original.append([mn,ops])
                fields = [(off,relocations[(section,off)]) for off in range(at,at+size) if (section,off) in relocations]
                for off,identity in fields:
                    literal = r'\$?0x[0-9a-f]+'
                    if len(re.findall(literal,ops)) == 1:
                        ops = re.sub(literal, lambda m:'@'+repr(identity), ops)
                    else:
                        ops += ' @'+repr(identity)
                if mn.startswith('j') or mn=='call':
                    number = re.match(r'^([0-9a-f]+)\b',ops)
                    if number:
                        target = int(number[1],16)
                        tag = 'insn:'+str(addresses[target]) if target in addresses else 'outside:'+str(target-lo)
                        if fields: tag = 'relocated'
                        ops = tag+ops[number.end():]
                if mn in ('sub','add') and re.fullmatch(r'\$0x[0-9a-f]+,%esp',ops):
                    frames.append([mn,int(ops.split(',')[0][1:],16)])
                    ops = '$FRAME,%esp'
                ops = MEMORY.sub('STACK(%esp)',ops)
                normalized.append([mn,ops])
                if mn=='call': calls.append([at-lo,fields,original[-1][1]])
            result[sym.name]={'size':sym['st_size'],'normalized':normalized,'original':original,'frames':frames,'calls':calls}
        return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-dir',type=Path,default=d.ROOT/'build/production-before')
    ap.add_argument('--out',type=Path,default=d.ROOT/'build/gcc3-alignment-stack-screen.json')
    ap.add_argument('--positive-control',type=Path,default=d.ROOT.parent/'gcc3-uref-stack-slot/build/gcc3-uref-stack-cross/V90AutoDigitalImpDetector/constant-0-rounding-1/candidate.o')
    args=ap.parse_args()
    objects=sorted(args.baseline_dir.glob('*.o'));assert len(objects)==300
    config=(args.baseline_dir/'.build-config').read_text();assert '-DDSPLIB_REPRODUCE_BUGS' in config
    ref=inventory(Path(d.b.BLOB));candidates=[];colour_candidates=[];near_candidates=[];common=0;same=0
    for obj in objects:
        for name,row in inventory(obj).items():
            if name not in ref:continue
            common+=1;original=ref[name]
            if row['frames']!=original['frames'] and row['calls'] and original['calls'] and max(row['size'],original['size']) <= 800:
                a=erase_colours(row['normalized']);z=erase_colours(original['normalized'])
                if abs(len(a)-len(z))<=2:
                    import difflib
                    changes=[op for op in difflib.SequenceMatcher(None,[repr(x) for x in a],[repr(x) for x in z],autojunk=False).get_opcodes() if op[0]!='equal']
                    residual=sum(max(op[2]-op[1],op[4]-op[3]) for op in changes)
                    if residual<=16:
                        near_candidates.append({'object':obj.name,'symbol':name,'residual_rows':residual,'blob_frames':original['frames'],'ours_frames':row['frames'],'blob_size':original['size'],'ours_size':row['size'],'blob_calls':original['calls'],'calls':row['calls'],'row_differences':[[original['normalized'][op[3]:op[4]],row['normalized'][op[1]:op[2]]] for op in changes]})
            if row['normalized']!=original['normalized']:
                if row['frames']!=original['frames'] and erase_colours(row['normalized'])==erase_colours(original['normalized']):
                    colour_candidates.append({'object':obj.name,'symbol':name,'blob_frames':original['frames'],'ours_frames':row['frames'],'blob_size':original['size'],'ours_size':row['size'],'calls':row['calls'],'blob_calls':original['calls']})
                continue
            same+=1
            if row['frames']==original['frames'] and row['original']==original['original']:continue
            if row['frames']==original['frames']:continue
            verdict=d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(obj),name))
            if verdict[0]=='EXACT':continue
            candidates.append({'object':obj.name,'symbol':name,'verdict':verdict,'blob_frames':original['frames'],'ours_frames':row['frames'],'blob_size':original['size'],'ours_size':row['size'],'calls':row['calls'],'blob_calls':original['calls']})
    # F11796 positive control: the archived double-half source has identical
    # non-stack instructions but retains the known-callee padding discrepancy.
    control=args.positive_control
    key='_ZN25V90AutoDigitalImpDetector10updateUrefEv'
    positive=inventory(control)[key];assert positive['normalized']==ref[key]['normalized']
    assert positive['frames']!=ref[key]['frames']
    bad=json.loads(json.dumps(positive['normalized']));bad[0][0]='unexpected'
    assert bad!=ref[key]['normalized']
    result={'revision':'2001434c','objects':len(objects),'common_defining_copies':common,'same_normalized_streams':same,'stack_only_candidates':len(candidates),'candidates':candidates,'colour_erased_stack_candidates':colour_candidates,'near_stack_candidates':near_candidates,'positive_control':str(control),'nonstack_change_refused':True,'config':config,'limits':'Stack-erased screening only; no source proof, no exact gain, no regalloc equivalence inferred.'}
    args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(common,'defining copies;',same,'normalized matches;',len(candidates),'stack-only candidates;1 positive/1 refusal control')
    print(len(near_candidates),'near-stack candidates;diagnostic only')
    for row in near_candidates: print(row['object'],row['symbol'],row['residual_rows'],row['blob_frames'],row['ours_frames'])
    print(len(colour_candidates),'colour-erased stack candidates;diagnostic only')
    for row in candidates+colour_candidates:print(row['object'],row['symbol'],row['blob_frames'],row['ours_frames'],row.get('verdict'))
if __name__=='__main__':main()
