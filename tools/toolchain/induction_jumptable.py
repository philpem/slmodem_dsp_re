"""Prove one absolute dispatch whose callee-saved index is bounded by its CFG.

The boundary is entry at a unique sized i386 function. Exact-value propagation
tracks the index and unsigned comparison flags; unknown branches explore both
edges. Calls require direct cdecl relocations and preserve callee-saved index.
No assumed table bound, source declaration or ignored relocation supplies proof.
"""
import io
import re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
import jumptable as j

INTEGER = frozenset(('mov', 'movl', 'movzbl', 'movzwl', 'movswl', 'movsbl',
                     'lea', 'inc', 'dec', 'add', 'sub', 'xor', 'and', 'or',
                     'not', 'neg', 'adc', 'sbb', 'cmp', 'cmpl', 'test',
                     'push', 'pop', 'call', 'ret', 'nop'))
FLOAT = frozenset(('fldz', 'fld1', 'fldl', 'flds', 'fildl', 'fildll',
                   'fstpl', 'fsts', 'fstps', 'faddl', 'fadds', 'faddp',
                   'fmull', 'fmuls', 'fmulp', 'fdivp', 'fdivrp', 'fdivrl',
                   'fdivl', 'fsubp', 'fsubrp', 'fsubrl', 'fsubl'))
CONDITIONAL = frozenset(('ja', 'jbe', 'jb', 'jae', 'je', 'jne', 'jz', 'jnz'))


def propagate(rows, start, dispatch, index, destinations, calls):
    """Return every proved dispatch value; refuse unknown/out-of-range values."""
    by_address = {r[0]: r for r in rows}
    alias = {'ebx':{'ebx','bx','bl','bh'}, 'esi':{'esi','si'},
             'edi':{'edi','di'}, 'ebp':{'ebp','bp'}}[index]
    saved={}; prefix_depth=0
    for _,raw,m,o in rows:
        if m in ('fldz','fld1'):continue
        if m!='push':break
        j.require(len(raw)==1 and raw[0] in range(0x50,0x58) and
                  o in ('%ebx','%esi','%edi','%ebp'), 'induction entry save form')
        prefix_depth+=4; saved[prefix_depth]=o
    j.require('%'+index in saved.values(),'induction index not saved at entry')
    pending = [(start, None, None, 0)]
    seen, used = set(), set()
    while pending:
        address, value, comparison, depth = pending.pop()
        state = (address, value, comparison, depth)
        if state in seen:
            continue
        seen.add(state)
        j.require(len(seen) <= 20000, 'induction state domain exceeds bound')
        j.require(address in by_address, 'induction successor outside instructions')
        _, raw, mnemonic, operands = by_address[address]
        j.require(re.search(r'%(sp|ss|cs|ds|es|fs|gs)\b', operands) is None,
                  'induction partial-stack/segment operand')
        if address == dispatch:
            j.require(value is not None and 0 <= value < len(destinations),
                      'unknown/out-of-range induction dispatch')
            used.add(value)
            pending.append((destinations[value], value, comparison, depth))
            continue
        if mnemonic == 'ret':
            j.require(depth == 0, 'induction unbalanced return stack')
            continue
        if mnemonic == 'push':
            j.require(len(raw)==1 and raw[0] in range(0x50,0x58) and
                      re.fullmatch(r'%(eax|ebx|ecx|edx|esi|edi|ebp)',operands),
                      'induction nonword push/stack address')
            depth += 4
        elif mnemonic == 'pop':
            j.require(len(raw)==1 and raw[0] in range(0x58,0x60) and
                      re.fullmatch(r'%(eax|ebx|ecx|edx|esi|edi|ebp)', operands),
                      'induction unsupported pop')
            j.require(depth not in saved or saved[depth]==operands,
                      'induction wrong saved-register restore')
            depth -= 4
        elif operands.endswith(',%esp'):
            adjust = re.fullmatch(r'\$0x([0-9a-f]+),%esp', operands)
            j.require(mnemonic in ('add','sub') and adjust is not None,
                      'induction unsupported stack adjustment')
            amount = int(adjust[1],16)
            if mnemonic=='add':
                j.require(not any(depth-amount<slot<=depth for slot in saved),
                          'induction discarded entry saves')
            depth += amount if mnemonic == 'sub' else -amount
        j.require(0 <= depth <= 1024 and depth % 4 == 0,
                  'induction stack depth')
        j.require(not (mnemonic == 'lea' and '%esp' in operands) and
                  not operands.startswith('%esp'), 'induction frame address escape')
        if '%esp' in operands and operands.endswith(')') and mnemonic not in ('cmp','cmpl','test'):
            slot = re.search(r'(?:^|,)(?:(0x[0-9a-f]+))?\(%esp\)$',operands)
            j.require(slot is not None,'induction complex frame write')
            width = 8 if mnemonic == 'fstpl' else 4
            offset = int(slot[1] or '0',16)
            j.require(offset + width <= depth,'induction return-address overwrite')
            j.require(not any(saved_depth<=depth and
                              offset<depth-saved_depth+4 and offset+width>depth-saved_depth
                              for saved_depth in saved), 'induction saved-register overwrite')
        if mnemonic == 'call':
            j.require(address in calls, 'unproved induction ABI call')
            comparison = None
        elif mnemonic in ('cmp', 'cmpl'):
            match = re.fullmatch(r'\$0x([0-9a-f]+),%' + index, operands)
            comparison = (value, int(match[1], 16)) if match and value is not None else None
        elif mnemonic not in FLOAT and mnemonic not in ('mov', 'movl', 'movzbl',
                'movzwl', 'movswl', 'movsbl', 'lea', 'push', 'pop', 'nop',
                'jmp') and mnemonic not in CONDITIONAL:
            comparison = None
        if mnemonic not in FLOAT and mnemonic not in ('cmp', 'cmpl', 'test',
                'push', 'call', 'jmp') and mnemonic not in CONDITIONAL:
            written = re.search(r'(?:^|,)%(\w+)$', operands)
            if written and written[1] in alias:
                full = written[1] == index
                if full and mnemonic == 'xor' and operands == '%%%s,%%%s' % (index,index):
                    value = 0
                elif full and mnemonic in ('mov', 'movl') and operands.startswith('$0x'):
                    value = int(operands.split(',')[0][1:], 16)
                elif full and mnemonic in ('inc', 'dec') and value is not None:
                    value = (value + (1 if mnemonic == 'inc' else -1)) & 0xffffffff
                elif full and mnemonic in ('add', 'sub') and value is not None and re.fullmatch(r'\$0x[0-9a-f]+,%'+index, operands):
                    amount = int(operands.split(',')[0][1:],16)
                    value = (value + (amount if mnemonic == 'add' else -amount)) & 0xffffffff
                elif full and mnemonic == 'lea' and operands == '0x0(%%%s,%%eiz,1),%%%s' % (index,index):
                    pass
                else:
                    value = None
        if mnemonic == 'jmp' or mnemonic in CONDITIONAL:
            target = re.match(r'^([0-9a-f]+)\s', operands)
            j.require(target is not None, 'undecoded induction branch')
            target = int(target[1], 16)
            if mnemonic == 'jmp':
                pending.append((target, value, comparison, depth))
                continue
            decision = None
            if comparison is not None:
                a, b = comparison
                decision = {'ja':a>b, 'jbe':a<=b, 'jb':a<b, 'jae':a>=b,
                            'je':a==b, 'jz':a==b, 'jne':a!=b, 'jnz':a!=b}[mnemonic]
            if decision is not False:
                pending.append((target,value,comparison,depth))
            if decision is True:
                continue
        pending.append((address+len(raw),value,comparison,depth))
    j.require(used == set(range(len(destinations))), 'unreached induction table entries')
    return {'states':len(seen), 'dispatch_values':sorted(used)}


def prove(path, name):
    elf = ELFFile(io.BytesIO(Path(path).read_bytes()))
    j.require(elf.elfclass==32 and elf.little_endian and elf['e_machine']=='EM_386'
              and elf['e_type']=='ET_REL', 'induction ELF format')
    sections = list(elf.iter_sections()); tab = elf.get_section_by_name('.symtab')
    j.require(tab is not None, 'induction missing symtab')
    symbols = list(tab.iter_symbols())
    functions = [s for s in symbols if s['st_info']['type']=='STT_FUNC'
                 and isinstance(s['st_shndx'],int) and s['st_size']]
    owners = [s for s in functions if s.name==name]
    j.require(len(owners)==1, 'induction ambiguous owner')
    owner=owners[0]; section_id=owner['st_shndx']; section=sections[section_id]
    start=owner['st_value']; end=start+owner['st_size']
    j.require(sum(s['st_shndx']==section_id and s['st_value']<end and
                  s['st_value']+s['st_size']>start for s in functions)==1,
              'induction overlapping owner')
    j.require(not any(s['st_info']['type']=='STT_FUNC' and not s['st_size']
                      and s['st_shndx']==section_id and start<=s['st_value']<end
                      for s in symbols), 'induction zero-sized function alias')
    j.require(not any(s['st_shndx']==section_id and start<s['st_value']<end
                      and s['st_info']['type']!='STT_SECTION' for s in symbols),
              'induction named interior entry')
    j.require(sum(s.name==section.name for s in sections)==1 and
              section['sh_type']=='SHT_PROGBITS' and section['sh_flags']&4,
              'induction code section')
    rows=j.instructions(str(path),section.name,start,end)
    indirect=[r for r in rows if r[2].startswith(('j','call')) and r[3].startswith('*')]
    j.require(len(indirect)==1, 'induction dispatch count')
    jump=indirect[0]
    match=re.fullmatch(r'\*0x[0-9a-f]+\(,%(ebx|esi|edi|ebp),4\)',jump[3])
    j.require(jump[2]=='jmp' and match is not None,'induction dispatch form')
    index=match[1]
    j.require(all(m in INTEGER or m in FLOAT or m=='jmp' or m in CONDITIONAL
                  for _,_,m,_ in rows), 'induction unsupported instruction')
    j.require(not any(m=='ret' and o for _,_,m,o in rows),'induction return form')
    bounds=[int(m[1],16)+1 for _,_,op,o in rows
            if op=='cmp' and (m:=re.fullmatch(r'\$0x([0-9a-f]+),%'+index,o))]
    j.require(len(bounds)==1 and 1<=bounds[0]<=256,'induction comparison extent')
    count=bounds[0]
    relocations=[]
    for rs in sections:
        if not isinstance(rs,RelocationSection): continue
        j.require(not rs.is_RELA(),'induction RELA')
        linked=sections[rs['sh_link']]
        for r in rs.iter_relocations():
            j.require(r['r_offset']+4<=sections[rs['sh_info']]['sh_size'],
                      'induction relocation extent')
            target=linked.get_symbol(r['r_info_sym'])
            j.require(target['st_shndx']!=section_id or r['r_info_type'] in (1,2),
                      'induction unsupported incoming relocation kind')
            relocations.append((rs['sh_info'],r,target))
    dispatch_relocs=[(r,s) for sec,r,s in relocations if sec==section_id
                     and jump[0]<=r['r_offset']<jump[0]+len(jump[1])]
    j.require(len(dispatch_relocs)==1,'induction dispatch relocation count')
    r,s=dispatch_relocs[0]
    j.require(r['r_info_type']==1 and r['r_offset']==jump[0]+len(jump[1])-4
              and s['st_info']['type']=='STT_SECTION' and isinstance(s['st_shndx'],int),
              'induction dispatch relocation form')
    table_id=s['st_shndx']; table=sections[table_id]
    j.require(table['sh_type']=='SHT_PROGBITS' and table['sh_flags']&2 and
              not table['sh_flags']&(1|4|16) and sum(x.name==table.name for x in sections)==1,
              'induction table flags')
    base=int.from_bytes(jump[1][-4:],'little')+s['st_value']; limit=base+count*4
    j.require(base%4==0 and limit<=table['sh_size'],'induction table extent')
    j.require(not any(s['st_shndx']==table_id and s['st_info']['type'] in
                      ('STT_OBJECT','STT_FUNC') and s['st_size'] and
                      s['st_value']<limit and s['st_value']+s['st_size']>base
                      for s in symbols), 'induction named table overlap')
    entries=[(r,s) for sec,r,s in relocations if sec==table_id and
             r['r_offset']<limit and r['r_offset']+4>base]
    j.require(len(entries)==count and sorted(r['r_offset'] for r,s in entries)==list(range(base,limit,4)),
              'induction entry relocation count')
    boundaries={r[0] for r in rows}; destinations=[]
    for r,s in sorted(entries,key=lambda x:x[0]['r_offset']):
        j.require(r['r_info_type']==1 and s['st_shndx']==section_id and
                  s['st_info']['type']=='STT_SECTION','induction entry form')
        address=int.from_bytes(table.data()[r['r_offset']:r['r_offset']+4],'little')+s['st_value']
        j.require(address in boundaries,'induction entry boundary')
        destinations.append(address)
    calls=set();occupied=set()
    for sec,r,s in relocations:
        if sec==section_id and start<=r['r_offset']<end:
            fields=set(range(r['r_offset'],r['r_offset']+4))
            j.require(not fields&occupied,'induction overlapping owner relocation')
            occupied.update(fields)
            matches=[row for row in rows if row[0]<=r['r_offset'] and r['r_offset']+4<=row[0]+len(row[1])]
            j.require(len(matches)==1,'induction undecoded relocation')
            address,raw,m,o=matches[0]
            if m=='call':
                j.require(raw[:1]==b'\xe8' and len(raw)==5 and r['r_offset']==address+1
                          and r['r_info_type']==2 and raw[1:]==b'\xfc\xff\xff\xff'
                          and s.name and s['st_shndx']!='SHN_ABS'
                          and s['st_info']['type'] in ('STT_FUNC','STT_NOTYPE'),
                          'induction ABI call relocation')
                calls.add(address)
            else:
                j.require(r['r_info_type']==1 and
                          ((address==jump[0]) or (m in FLOAT and o.startswith('0x')
                                                 and r['r_offset']==address+len(raw)-4)),
                          'induction unsupported data/control relocation')
        if s['st_shndx']==section_id and r['r_info_type'] in (1,2):
            data=sections[sec].data(); field=r['r_offset']
            raw=data[field:field+4]
            target=int.from_bytes(raw,'little',signed=True)+s['st_value']
            # A direct CALL's -4 is its PC bias, not an interior entry four
            # bytes before the named callee. Other PC32 consumers stay refused
            # under either interpretation.
            named_call=(r['r_info_type']==2 and s['st_info']['type']=='STT_FUNC'
                        and field>0 and data[field-1:field]==b'\xe8'
                        and raw==b'\xfc\xff\xff\xff')
            targets={target} if r['r_info_type']==1 else {target+4} if named_call else {target,target+4}
            if any(start<t<end for t in targets):
                j.require(sec==table_id and base<=r['r_offset']<limit,
                          'induction relocated interior entry')
    j.require(all(address in calls for address,_,m,_ in rows if m=='call'),
              'induction unrelocated call')
    j.require(not any(start<t<end and not start<=a<end for a,t in
                      j.section_direct_edges(str(path),section.name)),
              'induction external direct interior entry')
    state=propagate(rows,start,jump[0],index,destinations,calls)
    offsets=[t-start for t in destinations]
    return {'function':name,'count':count,'targets':offsets,'table_section':table.name,
            'table_offset':base,'relocation_offset':dispatch_relocs[0][0]['r_offset']-start,
            'index':index,'induction':state,
            'identity':('jump-table','R_386_32',count,tuple((name,t) for t in offsets))}
