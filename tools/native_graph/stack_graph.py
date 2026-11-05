"""Fixed-entry-frame stack-cell and call-result branch timing diagnostic.

Single adjacent CMP/JA/table dispatch admitted. Table bytes, relocations, count,
section and instruction boundaries validated, no indexed dispatch semantics proof.
All table successors are conservative CFG edges. Stack addresses are local only;
Writes to ESP except a fixed prologue/epilogue are refused. Frame-address
escapes are counted; callee writes through escaped pointers are unmodeled.
"""
import collections,io,re,sys
if __package__:
    from . import effect_graph as eg
else:
    import effect_graph as eg
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def graph(path,symbol):
    eg.validate_object(path)
    owned=eg.bi.owned_function(str(path),symbol)
    eg.need(owned is not None,'unique function ownership unavailable')
    section,start,data,relocs=owned; rows=eg.jt.instructions(str(path),section,start,start+len(data)); by={r[0]:r for r in rows}; succ={}; tables={}; guards={}
    body,rr=eg.bi.body(str(path),symbol)
    elf=ELFFile(io.BytesIO(path.read_bytes())); sec=elf.get_section_by_name(section); sections=list(elf.iter_sections()); sid=next(i for i,s in enumerate(sections) if s.name==section); tab=elf.get_section_by_name('.symtab')
    callnames={}
    for i,(at,raw,mn,text) in enumerate(rows):
        nxt=at+len(raw); edges=[] if mn=='ret' else [nxt] if nxt in by else []
        if mn=='call':
            targets=[v for off,v in rr.items() if at-start<=off<at-start+len(raw)]
            if len(targets)==1: callnames[at]=eg.key(targets[0])
        if mn.startswith('j'):
            m=re.match(r'^([0-9a-f]+)(?:\s|$)',text)
            if m:
                eg.need(not any(at<=start+offset<nxt for offset in relocs),'relocated direct CFG branch unsupported')
                target=int(m[1],16); eg.need(target in by,'external/interior CFG branch'); edges=[target] if mn=='jmp' else edges+[target]
            else:
                match=re.fullmatch(r'\*0x([0-9a-f]+)\(,%(e\w+),4\)',text)
                eg.need(mn=='jmp' and match is not None and i>=2,'unsupported indirect CFG')
                cp,jump=rows[i-2:i]; cm=re.fullmatch(r'\$0x([0-9a-f]+),%'+match[2],cp[3]); eg.need(cp[2]=='cmp' and cm is not None and jump[2]=='ja','unsupported dispatch guard')
                eg.need(not any(cp[0]<=start+off<jump[0]+len(jump[1]) for off in relocs),'relocated table guard unsupported')
                guards[at]=(cp[0],jump[0])
                count=int(cm[1],16)+1; eg.need(0<count<=256,'table count bound')
                code_reloc=[r for s in sections if isinstance(s,RelocationSection) and s['sh_info']==sid for r in s.iter_relocations() if at<=r['r_offset']<nxt]
                eg.need(len(code_reloc)==1,'dispatch relocation count'); cr=code_reloc[0]; ts=tab.get_symbol(cr['r_info_sym']); eg.need(cr['r_info_type']==1 and cr['r_offset']==at+len(raw)-4 and ts['st_info']['type']=='STT_SECTION' and isinstance(ts['st_shndx'],int),'dispatch relocation form')
                ti=ts['st_shndx']; tsec=sections[ti]; offset=int(match[1],16)+ts['st_value']; rel={r['r_offset']:r for s in sections if isinstance(s,RelocationSection) and s['sh_info']==ti for r in s.iter_relocations()}; edges=[]
                eg.need(offset+count*4<=tsec['sh_size'],'table extent')
                for off in range(offset,offset+count*4,4):
                    eg.need(off in rel,'missing table relocation'); r=rel[off]; target_sym=tab.get_symbol(r['r_info_sym']); eg.need(r['r_info_type']==1 and target_sym['st_info']['type']=='STT_SECTION' and target_sym['st_shndx']==sid,'table entry owner')
                    dst=int.from_bytes(tsec.data()[off:off+4],'little')+target_sym['st_value']; eg.need(dst in by,'table target instruction boundary'); edges.append(dst)
                eg.need(not tables,'multiple dispatch tables unsupported'); tables[at]=edges
        eg.need(mn=='ret' or mn=='jmp' or nxt in by,'out-of-function fallthrough'); succ[at]=edges
    predecessors=collections.defaultdict(list)
    for source,targets in succ.items():
        for target in targets:predecessors[target].append(source)
    for dispatch,(comparison,guard) in guards.items():
        eg.need(predecessors[guard]==[comparison] and succ[comparison]==[guard] and predecessors[dispatch]==[guard] and succ[guard][0]==dispatch and succ[guard][-1]!=dispatch,'table guard bypass or nonunique predecessor')
    return rows,succ,callnames,tables

def extract(path,symbol):
    try: rows,succ,callnames,tables=graph(path,symbol)
    except (ValueError,KeyError) as e: return {'status':'refused','reason':str(e)}
    start=rows[0][0]; by={r[0]:r for r in rows}
    relocated_fields=eg.bi.owned_function(str(path),symbol)[3]
    relocated_non_calls={at for at,raw,mn,text in rows if mn!='call' and any(at<=start+off<at+len(raw) for off in relocated_fields)}
    first=rows[0]; fm=re.fullmatch(r'\$0x([0-9a-f]+),%esp',first[3])
    if first[2]!='sub' or fm is None: return {'status':'refused','reason':'single entry SUB ESP frame required'}
    frame=int(fm[1],16)
    for at,raw,mn,text in rows:
        ops=eg.ot.parts(text)
        if '%esp' in text and re.search(r'(?:^|,)%e?sp$',text):
            if not (mn in ('sub','add') and text=='$0x%x,%%esp'%frame and (at==start or mn=='add')): return {'status':'refused','reason':'unsupported ESP adjustment/frame escape'}
        if mn in ('push','pop','leave','enter') or mn.startswith('loop'): return {'status':'refused','reason':'dynamic stack/control unsupported'}
    # Prove one stack depth per CFG join and balanced cdecl returns.
    depths={start:0};pending=collections.deque([start])
    while pending:
        at=pending.popleft();row=by[at];depth=depths[at]
        if row[2] in ('add','sub') and row[3]=='$0x%x,%%esp'%frame:depth+=frame if row[2]=='add' else -frame
        if row[2]=='ret' and depth!=0:return {'status':'refused','reason':'unbalanced fixed-frame return'}
        for target in succ[at]:
            if target in depths:
                if depths[target]!=depth:return {'status':'refused','reason':'ambiguous stack depth at join'}
            else:depths[target]=depth;pending.append(target)
    # Static case reachability is an anchor, not a dynamic iteration identity.
    cases=collections.defaultdict(list)
    for dispatch,targets in tables.items():
        for index,target in enumerate(targets):
            seen=set(); q=[target]
            while q:
                n=q.pop()
                if n in seen or n==dispatch: continue
                seen.add(n); cases[n].append(index); q.extend(succ[n])
    calls={at:('call',name,tuple(cases[at])) for at,name in callnames.items()}
    callcounts=collections.Counter(calls.values())
    predecessors=collections.defaultdict(list)
    for source,targets in succ.items():
        for target in targets:predecessors[target].append(source)
    unknown=('unknown',)
    def full_reg(op):
        lane=eg.ot.LANES.get(op[1:]) if op.startswith('%') else None
        return lane[0] if lane and lane[1:]==(32,0) else None
    def reg(op):
        lane=eg.ot.LANES.get(op[1:]) if op.startswith('%') else None
        return lane[0] if lane else None
    def val(op,regs):
        if op.startswith('$'):
            n=eg.ot.number(op[1:]); return ('constant',n) if n is not None else unknown
        r=full_reg(op); return regs.get(r,unknown) if r else unknown
    # Constants and call-result copies only. Unknown arithmetic kills written
    # registers; call clobbers eax/ecx/edx, preserving callee saves as cdecl.
    def transfer(row,regs):
        at,raw,mn,text=row; ops=eg.ot.parts(text); out=dict(regs)
        if at in relocated_non_calls:
            out={r:unknown for r in out}
        elif mn=='call':
            for r in ('eax','ecx','edx'): out[r]=unknown
            if at in calls and callcounts[calls[at]]==1: out['eax']=('call-result',calls[at])
        elif mn in ('mov','movl') and len(ops)==2 and full_reg(ops[1]): out[full_reg(ops[1])]=val(ops[0],regs)
        elif mn=='xor' and len(ops)==2 and ops[0]==ops[1] and full_reg(ops[1]): out[full_reg(ops[1])]=('constant',0)
        elif mn.startswith('j') or mn in ('cmp','cmpl','cmpw','cmpb','test','testl','testw','testb','ret','nop'):
            pass
        elif mn in ('mov','movl','movw','movb','movzbl','movzwl','movsbl','movswl','lea','inc','dec','neg','not','xor','or','and','add','sub','adc','sbb','shl','shr','sar','sal','rol','ror','rcl','rcr','bswap') or mn.startswith(('set','cmov')) or mn=='imul' and len(ops)>1:
            if ops and reg(ops[-1]) and reg(ops[-1])!='esp':out[reg(ops[-1])]=unknown
        else:
            # Implicit/multiple register outputs are not inferred from the
            # final printed operand (MUL, XCHG, CPUID, string operations, ...).
            out={r:unknown for r in out}
        return out
    incoming={start:{}}; q=collections.deque([start]); steps=0
    while q:
        at=q.popleft();steps+=1
        if steps>100000:return {'status':'refused','reason':'constant convergence bound'}
        out=transfer(by[at],incoming[at])
        for dst in succ[at]:
            old=incoming.get(dst); merged=out if old is None else {r:out.get(r,unknown) if out.get(r,unknown)==old.get(r,unknown) else unknown for r in set(out)|set(old)}
            if old!=merged:incoming[dst]=merged;q.append(dst)
    cells=collections.defaultdict(list); branches=[]
    sm=re.compile(r'(?:(0x[0-9a-f]+))?\(%esp\)$')
    for i,row in enumerate(rows):
        at,raw,mn,text=row
        if at not in incoming:continue
        ops=eg.ot.parts(text)
        if len(ops)==2:
            for oi,op in enumerate(ops):
                match=sm.fullmatch(op)
                if match:
                    if depths[at]!=-frame:continue
                    offset=int(match[1] or '0',16)+depths[at]
                    bits=eg.ot.width_bits(mn,ops)
                    if mn in ('cmp','cmpl','cmpw','cmpb','test','testl','testw','testb'):bits=8 if mn.endswith('b') else 16 if mn.endswith('w') else 32; kind='compare'
                    elif mn in ('mov','movl','movw','movb'): kind='write' if oi==1 else 'read'
                    else:continue
                    value=unknown if at in relocated_non_calls else val(ops[1-oi],incoming[at]); cells[offset].append({'at':at,'kind':kind,'width':bits,'value':value,'cases':tuple(cases[at])})
        if mn.startswith('j') and mn!='jmp' and i>0:
            prev=rows[i-1];pm=prev[2];pop=eg.ot.parts(prev[3])
            if pm in ('test','cmp') and prev[0] in incoming and len(pop)==2 and predecessors[at]==[prev[0]] and succ[prev[0]]==[at] and prev[0] not in relocated_non_calls:
                values=[val(p,incoming[prev[0]]) for p in pop]
                if all(v!=unknown for v in values): branches.append({'at':at,'kind':'branch','anchor':('branch',pm,tuple(values),tuple(cases[at])),'predicate':mn,'arms':{'fallthrough':succ[at][0],'taken':succ[at][-1]}})
    fingerprints=collections.defaultdict(list)
    for off,events in cells.items():
        extent=max((e['width'] or 0)//8 for e in events)
        if any(other!=off and off<other+max((e['width'] or 0)//8 for e in es) and other<off+extent for other,es in cells.items()):continue
        writes=[e for e in events if e['kind']=='write']
        if not writes or any(e['value'][0]!='constant' for e in writes):continue
        # Exclude outgoing argument area and permit local layout renaming.
        if off < -frame+16 or off>=0:continue
        signature=tuple(sorted((e['kind'],e['width'],e['value']) for e in events))
        fingerprints[signature].append((off,events))
    events=list(branches)
    for at,anchor in calls.items():
        events.append({'at':at,'kind':'call','anchor':anchor})
    supported_cells=[];ambiguous=[]
    for fp,owners in fingerprints.items():
        if len(owners)!=1: ambiguous.append({'fingerprint':fp,'offsets':[o[0] for o in owners]});continue
        off,es=owners[0]; supported_cells.append({'offset':off,'fingerprint':fp,'events':es})
        for e in es:
            if e['kind']=='write':events.append(dict(e,owner=('stack-cell',fp),anchor=('stack-write',fp,e['width'],e['value'],e['cases'])))
    return {'status':'bounded','instruction_count':len(rows),'events':events,'successors':succ,'entry':start,'frame':frame,'cells':supported_cells,'ambiguous_cells':ambiguous,'all_stack_cell_count':len(cells),'table_count':len(tables),'branches_supported':len(branches),'branches_total':sum(r[2].startswith('j') and r[2]!='jmp' for r in rows),'calls':len(calls),'relocated_noncall_instruction_sites':sorted(relocated_non_calls),'case_regions':dict(cases),'frame_address_escapes':[r[0] for r in rows if r[2]=='lea' and '%esp' in r[3]],'unknown':{}}

def compare(a,b):return eg.compare(a,b)
