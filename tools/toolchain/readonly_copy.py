"""Bounded ELF32 leaf proof for a forward anonymous REP MOVSL initializer.

Only the consumed literal operand receives a content identity. A decoded
straight-line setup, private fixed frame, immediate source kill and complete
leaf CFG must be proved. Unsupported forms are refusals, never byte matches.
"""
import re
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from jumptable import instructions, section_direct_edges, Refused, require

CALLEE = frozenset(('ebx','esi','edi','ebp'))
GPR = frozenset(('eax','ebx','ecx','edx','esi','edi','ebp','esp'))
SCALAR = ('scalar',)
ALIASES = {'al':'eax','ah':'eax','ax':'eax','bl':'ebx','bh':'ebx','bx':'ebx',
           'cl':'ecx','ch':'ecx','cx':'ecx','dl':'edx','dh':'edx','dx':'edx',
           'si':'esi','di':'edi','bp':'ebp','sp':'esp'}


def registers(operand):
    found = [ALIASES.get(r,r) for r in re.findall(r'%([a-z0-9]+)',operand) if r != 'eiz']
    require(all(r in GPR for r in found),'unknown/segment register')
    return found


def pointer(value):
    return value[0] in ('stack','literal')


def prove(path, name):
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        require(elf.elfclass == 32 and elf.little_endian and elf['e_type'] == 'ET_REL'
                and elf['e_machine'] == 'EM_386','unsupported ELF/ABI')
        tab = elf.get_section_by_name('.symtab')
        require(tab is not None,'missing symbol table')
        symbol_index = elf.get_section_index('.symtab')
        for sec in elf.iter_sections():
            if isinstance(sec,RelocationSection):
                require(sec['sh_link'] == symbol_index and sec['sh_type'] == 'SHT_REL'
                        and sec['sh_entsize'] == 8,'unsupported relocation table/REL semantics')
        symbols = list(tab.iter_symbols())
        matches = [s for s in symbols if s.name == name and s['st_info']['type'] == 'STT_FUNC']
        require(len(matches) == 1,'ambiguous function')
        fn = matches[0]
        require(isinstance(fn['st_shndx'],int) and fn['st_size'] > 0,'unsized function')
        section = elf.get_section(fn['st_shndx'])
        require(sum(s.name == section.name for s in elf.iter_sections()) == 1,'ambiguous code section name')
        start, end = fn['st_value'], fn['st_value']+fn['st_size']
        require(section['sh_flags'] & 4 and end <= section['sh_size'],'function bounds')
        for symbol in symbols:
            if symbol['st_shndx'] != fn['st_shndx'] or symbol['st_info']['type'] == 'STT_SECTION':
                continue
            require(not start < symbol['st_value'] < end,'named interior entry')
            if symbol['st_info']['type'] == 'STT_FUNC':
                lo, size = symbol['st_value'], symbol['st_size']
                require((lo,size) == (start,end-start) or not size or lo+size <= start or end <= lo,
                        'overlapping function owner')
        rows = instructions(str(path),section.name,start,end)
        require(len(rows) >= 3,'short function')
        require(rows[0][2:] == ('cld',''),'DF not established at entry')
        allocation = re.fullmatch(r'\$0x([0-9a-f]+),%esp',rows[1][3])
        require(rows[1][2] == 'sub' and allocation is not None,'fixed frame allocation')
        frame = int(allocation[1],16)
        require(0 < frame <= 256 and frame % 4 == 0,'frame extent')
        copies = [i for i,row in enumerate(rows) if row[1] == b'\xf3\xa5']
        require(len(copies) == 1,'one default-width forward copy required')
        copy = copies[0]
        require(copy >= 2 and copy+1 < len(rows),'copy corridor extent')
        relocations = [(r,sec) for sec in elf.iter_sections() if isinstance(sec,RelocationSection)
                       and sec['sh_info'] == fn['st_shndx'] for r in sec.iter_relocations()
                       if start <= r['r_offset'] < end]
        require(len(relocations) == 1,'one literal relocation required')
        relocation, _ = relocations[0]
        for sec in elf.iter_sections():
            if isinstance(sec,RelocationSection) and sec['sh_info'] == fn['st_shndx']:
                for r in sec.iter_relocations():
                    off = r['r_offset']
                    require(r['r_info_type'] in (1,2) and not (off < start < off+4 or off < end < off+4),
                            'unknown width/cross-boundary code relocation')
        require(relocation['r_info_type'] == 1,'absolute literal address required')
        literal_operand = relocation['r_offset']
        state = {r:('original',r) if r in CALLEE else SCALAR for r in GPR}
        state['esp'] = ('stack',0)
        state['_flags'] = False
        saves = {}
        source_seen = False

        def scalar_sources(operand, regs):
            require(all(not pointer(regs[r]) for r in registers(operand)), 'pointer read/escape')

        def assign(regs, register, value):
            require(register in GPR and register != 'esp','unsupported register definition')
            if register in CALLEE and regs[register] == ('original',register):
                require(register in saves.values(),'callee register changed before save')
            regs[register] = value

        def step(row, regs, prefix=False):
            nonlocal source_seen
            at, raw, op, args = row
            # Recognize assembler alignment LEAs as no-ops, including on EDI.
            nop = re.fullmatch(r'0x0\(%(\w+)(?:,%eiz,1)?\),%\1',args)
            if op == 'nop' and args == '' or op == 'lea' and nop and nop[1] in GPR:
                return
            mov = re.fullmatch(r'\$0x([0-9a-f]+),%(\w+)',args)
            if op == 'mov' and mov:
                immediate, dest = int(mov[1],16), mov[2]
                if at+1 == literal_operand:
                    require(not source_seen and dest == 'esi' and len(raw) == 5 and raw[0] == 0xbe,
                            'literal source load encoding')
                    require(prefix,'literal load outside setup')
                    source_seen = True
                    assign(regs,dest,('literal',immediate))
                else:
                    assign(regs,dest,('constant',immediate))
                return
            save = re.fullmatch(r'%(\w+),0x([0-9a-f]+)\(%esp\)',args)
            if op == 'mov' and save:
                register, offset = save[1], int(save[2],16)
                require(prefix and register in CALLEE and regs[register] == ('original',register),
                        'non-save stack write or pointer spill')
                require(offset % 4 == 0 and offset+4 <= frame and offset not in saves
                        and register not in saves.values(),'ambiguous save slot')
                saves[offset] = register
                return
            load = re.fullmatch(r'0x([0-9a-f]+)\(%esp\),%(\w+)',args)
            if op == 'mov' and load:
                offset, dest = int(load[1],16), load[2]
                if offset in saves:
                    require(not prefix and saves[offset] == dest,'save slot read/alias')
                    assign(regs,dest,('original',dest))
                else:
                    require(offset >= frame+4,'unproved local stack load')
                    assign(regs,dest,SCALAR)
                return
            move = re.fullmatch(r'%(\w+),%(\w+)',args)
            if op == 'mov' and move:
                source, dest = move.groups()
                require(source in GPR and dest in GPR,'partial register move')
                if source == 'esp':
                    require(prefix and dest == 'edi' and not source_seen,'stack-address escape')
                    assign(regs,dest,('stack',0))
                else:
                    scalar_sources('%'+source,regs)
                    assign(regs,dest,regs[source])
                return
            zero = re.fullmatch(r'%(\w+),%\1',args)
            if op == 'xor' and zero:
                assign(regs,zero[1],('constant',0))
                regs['_flags'] = True
                return
            if op == 'cmp':
                if '(' in args:
                    require(not prefix and args.count('(') == 1 and '%esp' in args,
                            'unproved memory consumer')
                    require(re.fullmatch(r'%\w+,\(%esp,%\w+,4\)',args) is not None,
                            'unproved stack comparison')
                    scalar_sources(args.replace('%esp',''),regs)
                else:
                    scalar_sources(args,regs)
                regs['_flags'] = True
                return
            if op in ('sete','setne'):
                require(regs['_flags'],'SETcc without proved scalar flags')
                require(args in ('%al','%cl','%dl') and not pointer(regs[ALIASES[args[1:]]]),
                        'memory/partial-pointer SETcc')
                assign(regs,ALIASES[args[1:]],SCALAR)
                return
            if op == 'lea':
                match = re.fullmatch(r'(.+),%(\w+)',args)
                require(match is not None and match[2] in GPR,'LEA destination')
                scalar_sources(match[1],regs)
                assign(regs,match[2],SCALAR)
                return
            if op in ('inc','dec') and re.fullmatch(r'%\w+',args):
                require(regs['_flags'],'unproved preserved carry flag')
                scalar_sources(args,regs)
                assign(regs,args[1:],SCALAR)
                return
            raise Refused('unsupported leaf instruction: '+op+' '+args)

        for row in rows[2:copy]:
            step(row,state,True)
        require(source_seen and state['esi'][0] == 'literal','literal not live at copy')
        require(state['ecx'][0] == 'constant' and 0 < state['ecx'][1] <= 64,'unknown copy count')
        count = state['ecx'][1]
        extent = 4*count
        require(state['edi'] == ('stack',0) and extent <= frame,'private destination extent')
        require(all(extent <= offset for offset in saves),'copy overlaps saved registers')
        require(rows[copy][2:] == ('rep','movsl %ds:(%esi),%es:(%edi)'), 'copy segment/width override')
        source_offset = state['esi'][1]
        state['ecx'] = ('constant',0)
        state['edi'] = ('stack',extent)
        step(rows[copy+1],state)
        require(not pointer(state['esi']),'full immediate post-copy source kill required')
        # Decode both arms of every remaining branch. Scalar joins widen;
        # pointer joins must agree exactly, preserving all alias provenance.
        by_address = {row[0]:i for i,row in enumerate(rows)}
        pending = [(copy+2,state)]
        seen = {}
        returns = 0
        while pending:
            index, regs = pending.pop()
            require(copy+2 <= index < len(rows),'entry into copy corridor')
            if index in seen:
                old = seen[index]
                joined = {}
                for register in GPR:
                    a,b = old[register],regs[register]
                    require(a == b or not pointer(a) and not pointer(b),'ambiguous pointer join')
                    joined[register] = a if a == b else SCALAR
                joined['_flags'] = old['_flags'] and regs['_flags']
                if joined == old:
                    continue
                regs = joined
            else:
                regs = dict(regs)
            seen[index] = dict(regs)
            row = rows[index]
            at, raw, op, args = row
            free = re.fullmatch(r'\$0x([0-9a-f]+),%esp',args)
            if op == 'add' and free:
                require(int(free[1],16) == frame and index+1 == len(rows)-1
                        and rows[index+1][2:] == ('ret',''),'unbalanced frame release')
                require(all(regs[r] == ('original',r) for r in CALLEE),'callee-save not restored')
                returns += 1
                continue
            if op in ('ja','jae','jb','jbe','jg','jge','jl','jle','je','jne','js','jns','jmp'):
                require(op == 'jmp' or regs['_flags'],'branch without proved scalar flags')
                target = re.match(r'^([0-9a-f]+)(?:\s|$)',args)
                require(target is not None and int(target[1],16) in by_address,'nonlocal/indirect transfer')
                pending.append((by_address[int(target[1],16)],dict(regs)))
                if op != 'jmp':
                    pending.append((index+1,dict(regs)))
                continue
            step(row,regs)
            pending.append((index+1,regs))
        require(returns > 0,'no balanced return')
        require(set(seen) | {len(rows)-1} == set(range(copy+2,len(rows))),
                'unproved unreachable tail instructions')
        # Data extent, storage and ownership are independent of its contents.
        target = tab.get_symbol(relocation['r_info_sym'])
        require(target['st_info']['type'] == 'STT_SECTION' and target['st_value'] == 0,
                'not anonymous section storage')
        literal_index = target['st_shndx']
        require(isinstance(literal_index,int),'literal section index')
        literal = elf.get_section(literal_index)
        require(literal['sh_type'] == 'SHT_PROGBITS' and literal['sh_flags'] & 2
                and not literal['sh_flags'] & (1|4|16|32),'not ordinary readonly storage')
        require(source_offset+extent <= literal['sh_size'] and source_offset+extent <= 2**32,
                'literal extent')
        for symbol in symbols:
            if symbol['st_shndx'] == literal_index and symbol['st_info']['type'] != 'STT_SECTION':
                lo,size = symbol['st_value'],symbol['st_size']
                require(not (source_offset <= lo < source_offset+extent or size and
                    lo < source_offset+extent and source_offset < lo+size),'ambiguous literal owner')
        edges = section_direct_edges(str(path),section.name)
        require(not any(not start <= a < end and start < z < end for a,z in edges),
                'external direct interior entry')
        checked = 0
        for sec in elf.iter_sections():
            if not isinstance(sec,RelocationSection):
                continue
            owner = elf.get_section(sec['sh_info'])
            data = owner.data()
            for rel in sec.iter_relocations():
                off = rel['r_offset']
                if sec['sh_info'] == literal_index:
                    require(rel['r_info_type'] in (1,2),'unknown literal relocation width')
                    require(not source_offset-3 <= off < source_offset+extent,'relocated literal bytes')
                destination = tab.get_symbol(rel['r_info_sym'])
                if destination['st_shndx'] != fn['st_shndx']:
                    continue
                require(rel['r_info_type'] in (1,2) and off+4 <= len(data),'unknown text reference')
                addend = int.from_bytes(data[off:off+4],'little',signed=rel['r_info_type']==2)
                targets = [(destination['st_value']+addend) & 0xffffffff]
                if rel['r_info_type'] == 2:
                    targets.append((destination['st_value']+addend+4) & 0xffffffff)
                require(not any(start < z < end for z in targets),'relocated interior entry')
                checked += 1
        contents = literal.data()[source_offset:source_offset+extent]
        return {'relocation_offset':literal_operand-start,'table_section':literal.name,
            'table_offset':source_offset,'identity':('readonly-copy',4,count,contents),
            'count':count,'width':4,'frame':frame,'saved_slots':saves,
            'copy_offset':rows[copy][0]-start,'kill_offset':rows[copy+1][0]-start,
            'cfg_states':len(seen),'direct_edges_checked':len(edges),'text_references_checked':checked}
