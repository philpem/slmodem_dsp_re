#!/usr/bin/env python3
"""One-function i386 audit. Fail closed outside the decoded InitMOH vocabulary.

This is scratch measurement apparatus, not a replacement byte classifier.
All ELF bytes are read once; the JSON records the bytes actually decoded.
"""
from pathlib import Path
import copy, hashlib, io, json, subprocess, sys
sys.path.insert(0, str(Path(__file__).parent / 'deps'))
import z3 as z
import capstone as cs
from capstone.x86 import X86_OP_REG, X86_OP_IMM, X86_OP_MEM
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

HERE = Path(__file__).resolve().parent
NAME = 'VPcmV34InitMOH'
B = lambda x, n=32: z.BitVecVal(x, n)
S0 = B(0xf0000000)
G = B(0xe0000000)
D = B(0xe0000100)
O, C, SESSION, MSG = z.BitVecs('object config session message', 32)
LATE, FLAG = z.BitVecs('late flag', 8)
DEBUG = z.BitVec('debug_level', 32)
BASEMEM = z.Array('entry_memory', z.BitVecSort(32), z.BitVecSort(8))
REGS = ['eax','ecx','edx','ebx','esp','ebp','esi','edi']
FLAGS = ['CF','PF','AF','ZF','SF','OF']
ENTRY = {r:z.BitVec('entry_'+r,32) for r in REGS}
ENTRY['esp'] = S0
ALIASES = {}
for full, word, low, high in [('eax','ax','al','ah'),('ebx','bx','bl','bh'),('ecx','cx','cl','ch'),('edx','dx','dl','dh')]:
    ALIASES.update({full:(full,0,4),word:(full,0,2),low:(full,0,1),high:(full,1,1)})
for full,word in [('esi','si'),('edi','di'),('esp','sp'),('ebp','bp')]:
    ALIASES.update({full:(full,0,4),word:(full,0,2)})

def need(ok, message):
    if not ok: raise RuntimeError(message)

class AuditRefusal(RuntimeError):
    """A deliberate admission rejection, distinct from apparatus failures."""
    def __init__(self, code, message):
        super().__init__(message);self.code=code

def admit(ok, code, message):
    if not ok:raise AuditRefusal(code,message)

def simp(x): return z.simplify(x)

def load(mem, addr, size):
    return simp(z.Concat(*[z.Select(mem,simp(addr+B(k))) for k in reversed(range(size))])) if size>1 else simp(z.Select(mem,addr))

def store(mem, addr, value, size):
    value = simp(value)
    for k in range(size): mem=z.Store(mem,simp(addr+B(k)),simp(z.Extract(8*k+7,8*k,value)))
    return simp(mem)

INITIAL=BASEMEM
for addr,val,size in [(O+B(0xac3c),C,4),(O+B(0x3548),SESSION,4),(G,DEBUG,4)]:
    INITIAL=store(INITIAL,addr,val,size)
INITIAL_STACK={p:z.BitVec('entry_stack_'+str(p),8) for p in range(0xefffffe4,0xf0000014)}
for off,val,size in [(4,O,4),(8,MSG,4),(12,LATE,1),(16,FLAG,1)]:
    for k in range(size):INITIAL_STACK[0xf0000000+off+k]=simp(z.Extract(8*k+7,8*k,val))
DOMAIN=[z.UGE(O,B(0x1000)),z.ULE(O,B(0xd0000000)),z.UGE(C,B(0x1000)),z.ULE(C,B(0xd0000000)),z.UGE(SESSION,B(0x1000)),z.ULE(SESSION,B(0xd0000000))]

class Nodes:
    def __init__(self): self.by_id={}; self.nodes={}; self.expressions={}
    def add(self,expr):
        if expr is None: return None
        k=expr.get_id()
        if k in self.by_id:return self.by_id[k]
        # Z3 reuses AST IDs after the last reference is released. Retain the
        # actual AST as long as its ID participates in the emitted DAG.
        self.expressions[k]=expr
        children=[self.add(a) for a in expr.children()]
        n='e'+str(len(self.nodes)); self.by_id[k]=n
        self.nodes[n]={'operator':str(expr.decl()),'sort':str(expr.sort()),'args':children}
        if z.is_bv_value(expr):self.nodes[n]['value']=expr.as_long()
        elif not children:self.nodes[n]['symbol']=str(expr)
        # Extract and extension parameters are part of decl(), but spell them out.
        if expr.decl().params(): self.nodes[n]['parameters']=[str(p) for p in expr.decl().params()]
        return n
NODES=Nodes()

def elf_function(path, symbol=NAME):
    raw=Path(path).read_bytes(); elf=ELFFile(io.BytesIO(raw)); syms=elf.get_section_by_name('.symtab')
    admit(elf.elfclass==32 and elf.little_endian and elf['e_machine']=='EM_386' and elf['e_type']=='ET_REL','elf_architecture','requires little-endian ELF32 i386 relocatable input')
    hits=[s for s in syms.iter_symbols() if s.name==symbol and isinstance(s['st_shndx'],int) and s['st_size']]
    need(len(hits)==1,'function must have unique sized definition')
    sym=hits[0]; sec=elf.get_section(sym['st_shndx']); start=sym['st_value']; size=sym['st_size']; body=sec.data()[start:start+size]
    aliases=[s.name for s in syms.iter_symbols() if s.name!=symbol and s['st_shndx']==sym['st_shndx'] and start<=s['st_value']<start+size and (s['st_info']['type']=='STT_FUNC' or (s['st_info']['type']=='STT_NOTYPE' and s['st_info']['bind']!='STB_LOCAL'))]
    admit(not aliases,'alternate_entry','unsupported alternate/incoming function entry aliases: '+str(aliases))
    reloc={}
    for rs in elf.iter_sections():
        if isinstance(rs,RelocationSection) and rs['sh_info']==sym['st_shndx']:
            for r in rs.iter_relocations():
                if r['r_offset']<start+size and r['r_offset']+4>start:
                    admit(start<=r['r_offset'] and r['r_offset']+4<=start+size,'relocation_extent','straddling relocation')
                    admit(r['r_info_type'] in [1,2],'relocation_type','unsupported relocation type')
                    relative=r['r_offset']-start
                    admit(all(relative+4<=p or p+4<=relative for p in reloc),'relocation_overlap','duplicate or overlapping relocation fields')
                    target=syms.get_symbol(r['r_info_sym']); rel={'offset':r['r_offset']-start,'type':r['r_info_type'],'symbol':target.name,'addend':int.from_bytes(sec.data()[r['r_offset']:r['r_offset']+4],'little')}
                    if isinstance(target['st_shndx'],int) and elf.get_section(target['st_shndx'])['sh_flags']&2 and target['st_info']['type']=='STT_SECTION':
                        targetsec=elf.get_section(target['st_shndx']); data=targetsec.data(); p=rel['addend']; rel['target_section']=targetsec.name
                        if 'str' in targetsec.name:rel['string_hex']=data[p:data.index(b'\0',p)+1].hex()
                    reloc[r['r_offset']-start]=rel
    decoder=cs.Cs(cs.CS_ARCH_X86,cs.CS_MODE_32); decoder.detail=True
    ins=list(decoder.disasm(body,start)); need(sum(i.size for i in ins)==size,'undecoded bytes')
    rows=[]
    for i in ins:
        rr=[r for p,r in reloc.items() if i.address-start<=p<i.address-start+i.size]
        rows.append({'offset':i.address-start,'address':i.address,'size':i.size,'bytes':i.bytes.hex(),'mnemonic':i.mnemonic,'operands':i.op_str,'relocations':rr,'instruction':i})
        if rr:
            admit(len(rr)==1,'relocation_instruction_count','multiple relocation fields in one instruction')
            r=rr[0];p=i.address-start
            if i.mnemonic=='call':
                admit(i.bytes[0]==0xe8 and i.size==5 and r['offset']==p+1 and r['type']==2 and r['addend']==0xfffffffc,'call_relocation','unsupported call relocation site/type/addend; requires PC32 function entry (-4)')
            elif r['symbol']=='dsplibs_debug_level':
                admit(i.mnemonic=='cmp' and i.size==7 and r['offset']==p+2 and r['type']==1 and r['addend']==0 and i.operands[0].type==X86_OP_MEM and not i.operands[0].mem.base and not i.operands[0].mem.index,'debug_relocation','unsupported debug global relocation site/type/addend; requires R_386_32 +0')
            elif 'string_hex' in r:
                admit(i.mnemonic=='mov' and i.size==7 and r['offset']==p+3 and r['type']==1 and i.operands[0].type==X86_OP_MEM and i.reg_name(i.operands[0].mem.base)=='esp' and i.operands[0].mem.disp==0 and not i.operands[0].mem.index,'string_relocation','unsupported string relocation site/type')
            else:raise AuditRefusal('relocation_target','unsupported relocation target/effect')
    return {'path':str(path),'sha256':hashlib.sha256(raw).hexdigest(),'body_sha256':hashlib.sha256(body).hexdigest(),'start':start,'size':size,'rows':rows,'by_offset':{r['offset']:r for r in rows},'relocations':list(reloc.values())}

def cfg(fun):
    rows=fun['rows']; positions=set(fun['by_offset']); leaders={0}; succ={}
    for r in rows:
        i=r['instruction']; p=r['offset']; nxt=p+i.size; m=i.mnemonic
        if m in ['je','ja','jg','jmp']:
            target=i.operands[0].imm-fun['start']; need(target in positions,'branch target not instruction')
            succ[p]=[target]+([nxt] if m!='jmp' else []); leaders.add(target)
            if nxt in positions:leaders.add(nxt)
        elif m=='ret':
            succ[p]=[]
            if nxt in positions:leaders.add(nxt)
        else:succ[p]=[nxt] if nxt in positions else []
    blocks=[]; current=None
    for r in rows:
        if r['offset'] in leaders:
            current={'start':r['offset'],'instructions':[]}; blocks.append(current)
        current['instructions'].append(r['offset'])
    owner={p:b['start'] for b in blocks for p in b['instructions']}
    edges=[]
    for b in blocks:
        b['successors']=[owner[p] for p in succ[b['instructions'][-1]]]
        edges.extend((b['start'],x) for x in b['successors'])
    seen=set(); todo=[0]
    while todo:
        p=todo.pop()
        if p in seen:continue
        seen.add(p); todo.extend(succ[p])
    for b in blocks:b['reachable']=b['start'] in seen
    return {'blocks':blocks,'edges':edges,'reachable_instruction_offsets':sorted(seen),'unreachable_instruction_offsets':sorted(positions-seen),'reachable_edges':[e for e in edges if e[0] in seen]}

FEASIBILITY_CACHE={}
FEASIBILITY_STATS={'solver_queries':0,'cache_hits':0}
def feasible(constraints):
    key=tuple(simp(c).sexpr() for c in constraints)
    if key in FEASIBILITY_CACHE:
        FEASIBILITY_STATS['cache_hits']+=1;return FEASIBILITY_CACHE[key]
    FEASIBILITY_STATS['solver_queries']+=1
    s=z.Solver(); s.set(timeout=20000);s.add(*DOMAIN,*constraints); result=s.check()
    need(result!=z.unknown,'feasibility unknown: '+s.reason_unknown())
    FEASIBILITY_CACHE[key]=result==z.sat;return result==z.sat

PROOFS=[]
def prove(label, a, b, constraints):
    a=simp(a);b=simp(b)
    row={'label':label,'left':NODES.add(a),'right':NODES.add(b)}
    if z.eq(a,b):row['result']='identical-expression';PROOFS.append(row);return True
    s=z.Solver();s.set(timeout=30000);s.add(*DOMAIN,*constraints,a!=b);r=s.check();row['result']=str(r)
    if r==z.sat:row['counterexample']=str(s.model())
    elif r==z.unknown:row['reason']=s.reason_unknown()
    PROOFS.append(row);return r==z.unsat

class State:
    def __init__(self):
        self.reg=dict(ENTRY);self.flag={f:z.Bool('entry_'+f) for f in FLAGS};self.mem=INITIAL;self.stack=dict(INITIAL_STACK)
        self.defs={r:[f'entry:{r}:{k}' for k in range(4)] for r in REGS};self.fdefs={f:'entry:'+f for f in FLAGS}
        self.memdefs=[];self.constraints=[];self.branches=[];self.trace=[];self.calls=[];self.pc=0
    def clone(self):
        n=State();n.reg=dict(self.reg);n.flag=dict(self.flag);n.mem=self.mem;n.stack=dict(self.stack);n.defs={k:list(v) for k,v in self.defs.items()};n.fdefs=dict(self.fdefs)
        n.memdefs=list(self.memdefs);n.constraints=list(self.constraints);n.branches=list(self.branches);n.trace=list(self.trace);n.calls=list(self.calls);n.pc=self.pc;return n
    def use_reg(self,name,kind='data'):
        full,off,size=ALIASES[name];self.step['reads'].append({'kind':kind,'register':full,'bytes':list(range(off,off+size)),'definitions':self.defs[full][off:off+size]})
        return simp(z.Extract((off+size)*8-1,off*8,self.reg[full]))
    def write_reg(self,name,value,copy_from=None):
        full,off,size=ALIASES[name];value=simp(value)
        bits=[]
        for k in reversed(range(4)):
            bits.append(z.Extract(8*(k-off)+7,8*(k-off),value) if off<=k<off+size else z.Extract(8*k+7,8*k,self.reg[full]))
        self.reg[full]=simp(z.Concat(*bits))
        ids=[f'{self.pc:03x}:reg:{full}:{k}' for k in range(off,off+size)]
        self.defs[full][off:off+size]=ids
        self.step['writes'].append({'kind':'register','register':full,'bytes':list(range(off,off+size)),'definitions':ids,'value':NODES.add(value),'copy_from':copy_from})
        self.step['_outputs'].append(value)
        self.step['_output_descriptors'].append((full,off,size))
    def use_flag(self,f):
        self.step['reads'].append({'kind':'flag','flag':f,'definitions':[self.fdefs[f]]});return self.flag[f]
    def flags(self,values):
        for f,v in values.items():
            self.flag[f]=simp(v);self.fdefs[f]=f'{self.pc:03x}:flag:{f}';self.step['writes'].append({'kind':'flag','flag':f,'definitions':[self.fdefs[f]],'value':NODES.add(self.flag[f])})
            self.step['_flag_outputs'][f]=self.flag[f]
    def memory_read(self,addr,size,kind='data'):
        addr=simp(addr)
        if z.is_bv_value(addr) and 0xefffffe4<=addr.as_long()<0xf0000014:
            parts=[self.stack.get(addr.as_long()+k,z.BitVec('entry_stack_'+str(addr.as_long()+k),8)) for k in reversed(range(size))]
            val=simp(z.Concat(*parts)) if size>1 else parts[0]
        else:val=load(self.mem,addr,size)
        # A definite store-to-load relation is exact. Remaining possible alias
        # predecessors are listed conservatively, not silently discarded.
        origins=[]
        for k in range(size):
            p=simp(addr+B(k));defs=[]
            for old,identity in reversed(self.memdefs):
                if z.eq(p,old):defs.append(identity);break
                eq=simp(p==old)
                if z.is_false(eq):continue
                if feasible(self.constraints+[eq]):defs.append(identity+' [may-alias]')
            if not defs:defs=['entry-or-call-memory']
            origins.append(defs)
        self.step['reads'].append({'kind':kind,'memory_address':NODES.add(addr),'size':size,'value':NODES.add(val),'byte_definitions':origins})
        self.step['_loads'].append((addr,val));return val
    def memory_write(self,addr,value,size):
        value=simp(value);addr=simp(addr)
        if z.is_bv_value(addr) and 0xefffffe4<=addr.as_long()<0xf0000014:
            for k in range(size):self.stack[addr.as_long()+k]=simp(z.Extract(8*k+7,8*k,value))
        else:self.mem=store(self.mem,addr,value,size)
        ids=[]
        for k in range(size):
            identity=f'{self.pc:03x}:mem:{k}';ids.append(identity);self.memdefs.append((simp(addr+B(k)),identity))
        self.step['writes'].append({'kind':'memory','address':NODES.add(addr),'size':size,'value':NODES.add(value),'definitions':ids})
        self.step['_stores'].append((addr,value))

def parity(value):
    b=z.Extract(7,0,value);v=z.BoolVal(True)
    for k in range(8):v=z.Xor(v,z.Extract(k,k,b)==B(1,1))
    return v

def arithmetic_flags(a,b,res,sub=False,logic=False,identity=''):
    width=a.size();sgn=lambda v:z.Extract(width-1,width-1,v)==B(1,1)
    return {'CF':z.BoolVal(False) if logic else (z.ULT(a,b) if sub else z.ULT(res,a)),
            'PF':parity(res),'AF':z.Bool('undefined_AF_'+identity) if logic else z.Extract(4,4,a^b^res)==B(1,1),
            'ZF':res==B(0,width),'SF':sgn(res),'OF':z.BoolVal(False) if logic else (z.And(sgn(a)!=sgn(b),sgn(res)!=sgn(a)) if sub else z.And(sgn(a)==sgn(b),sgn(res)!=sgn(a)))}

def address(st,i,op,row):
    m=op.mem;v=B(m.disp)
    if row['relocations']:
        rr=row['relocations'][0]
        if rr['symbol']=='dsplibs_debug_level':v=G
    if m.base:v=v+st.use_reg(i.reg_name(m.base),'address')
    if m.index:v=v+st.use_reg(i.reg_name(m.index),'address')*B(m.scale)
    return simp(v)

def get(st,i,op,row):
    if op.type==X86_OP_REG:return st.use_reg(i.reg_name(op.reg))
    if op.type==X86_OP_IMM:
        if row['relocations'] and 'string_hex' in row['relocations'][0]:return D
        return B(op.imm,op.size*8)
    if op.type==X86_OP_MEM:return st.memory_read(address(st,i,op,row),op.size)
    raise RuntimeError('unsupported operand')

def put(st,i,op,row,value,copy_from=None):
    if op.type==X86_OP_REG:st.write_reg(i.reg_name(op.reg),value,copy_from)
    elif op.type==X86_OP_MEM:st.memory_write(address(st,i,op,row),value,op.size)
    else:raise RuntimeError('bad destination')

def execute(fun):
    done=[];todo=[State()]
    while todo:
        st=todo.pop()
        while True:
            row=fun['by_offset'][st.pc];i=row['instruction'];m=i.mnemonic;ops=i.operands
            st.step={'offset':st.pc,'reads':[],'writes':[],'_outputs':[],'_output_descriptors':[],'_flag_outputs':{},'_loads':[],'_stores':[]};step=st.step
            if m=='mov':
                val=get(st,i,ops[1],row);put(st,i,ops[0],row,val,{'operand':i.op_str,'kind':'byte-preserving-copy'})
            elif m in ['movzx','movsx']:
                val=get(st,i,ops[1],row);ext=ops[0].size*8-val.size();res=z.ZeroExt(ext,val) if m=='movzx' else z.SignExt(ext,val)
                put(st,i,ops[0],row,res,{'kind':m,'source_bytes':ops[1].size,'destination_bytes':ops[0].size,'upper_bytes':'zero' if m=='movzx' else 'replicated sign bit'})
            elif m=='lea':put(st,i,ops[0],row,address(st,i,ops[1],row),{'kind':'address-formation'})
            elif m in ['xor','and','add','sub','cmp','test']:
                zero=(m=='xor' and ops[0].type==X86_OP_REG and ops[1].type==X86_OP_REG and ops[0].reg==ops[1].reg)
                if zero:
                    a=b=B(0,ops[0].size*8);res=a;step['zero_idiom']=True
                else:
                    a=get(st,i,ops[0],row);b=get(st,i,ops[1],row)
                    if a.size()!=b.size():b=z.Extract(a.size()-1,0,b) if b.size()>a.size() else z.SignExt(a.size()-b.size(),b)
                    res={'xor':lambda:a^b,'and':lambda:a&b,'test':lambda:a&b,'add':lambda:a+b,'sub':lambda:a-b,'cmp':lambda:a-b}[m]()
                res=simp(res)
                if m not in ['cmp','test']:put(st,i,ops[0],row,res)
                st.flags(arithmetic_flags(a,b,res,sub=m in ['sub','cmp'],logic=m in ['xor','and','test'],identity=f'{st.pc:03x}'))
            elif m=='setne':put(st,i,ops[0],row,z.If(st.use_flag('ZF'),B(0,8),B(1,8)))
            elif m in ['je','ja','jg','jmp']:
                cond=z.BoolVal(True) if m=='jmp' else (st.use_flag('ZF') if m=='je' else (z.And(z.Not(st.use_flag('CF')),z.Not(st.use_flag('ZF'))) if m=='ja' else z.And(st.use_flag('SF')==st.use_flag('OF'),z.Not(st.use_flag('ZF')))))
                step['_condition']=simp(cond)
            elif m=='call':
                need(len(row['relocations'])==1,'unresolved call')
                target=row['relocations'][0]['symbol'];need(target in ['v34handshakinit','_ZN19GenericToneDetector5resetEv','dsplibs_debug_printf'],'unsupported call')
                narg=2 if target=='v34handshakinit' else 1;sp=st.use_reg('esp','address')
                args=[st.memory_read(sp+B(4*k),4,'call-argument') for k in range(narg)]
                call={'target':target,'offset':st.pc,'args':args,'memory':st.mem,'stack':dict(st.stack),'registers':dict(st.reg),'flags':dict(st.flag),'private_stack_range':[8,48]}
                st.calls.append(call);step['call_target']=target
                # Congruent opaque calls may write any external memory and their
                # outgoing argument cells. No address of the preserved frame is
                # passed. No callee implementation is assumed by this summary.
                st.mem=z.Array('after_'+target+'_memory',z.BitVecSort(32),z.BitVecSort(8))
                for k in range(8):st.stack[sp.as_long()+k]=z.BitVec('after_'+target+'_outgoing_'+str(k),8)
                st.memdefs=[(a,d) for a,d in st.memdefs if z.is_bv_value(a)]
                for r in ['eax','ecx','edx']:st.write_reg(r,z.BitVec('after_'+target+'_'+r,32))
                st.flags({f:z.Bool('after_'+target+'_'+f) for f in FLAGS})
                step['abi_havoc']={'registers':['eax','ecx','edx'],'flags':FLAGS,'external_memory':'arbitrary shared relational call result','x87':'arbitrary shared result, unused in caller','preserved':['ebx','esi','edi','ebp','esp'],'stack_preserved_current_sp_bytes':[8,48]}
            elif m=='ret':
                sp=st.use_reg('esp','address');step['_return_target']=st.memory_read(sp,4,'return-address');st.write_reg('esp',sp+B(4));step['_return']=True
            else:raise AuditRefusal('unsupported_instruction',f'unsupported {m} at {st.pc:x}')
            st.trace.append(step)
            if m=='ret':annotate_liveness(st);done.append(st);break
            if m in ['je','ja','jg']:
                for taken in [False,True]:
                    c=step['_condition'] if taken else z.Not(step['_condition'])
                    if feasible(st.constraints+[c]):
                        child=st.clone();child.constraints.append(c);child.branches.append({'offset':st.pc,'taken':taken,'predicate':NODES.add(step['_condition'])});child.pc=ops[0].imm-fun['start'] if taken else st.pc+i.size;todo.append(child)
                break
            st.pc=ops[0].imm-fun['start'] if m=='jmp' else st.pc+i.size
    return done

# Bijective definition/use mapping, keyed by each definition's actual consumers.
# These swaps are audited as values, rather than guessing one global register map.
MAP={0x078:0x07a,0x07e:0x078,0x080:0x082,0x087:0x080,0x089:0x08b,0x090:0x089,
     0x0bf:0x0c1,0x0c1:0x0ca,0x0ca:0x0bf,0x0d1:0x0de,0x0d8:0x0e7,0x0de:0x0d8,0x0e4:0x0d1,0x0eb:0x0e5,
     0x10f:0x115,0x115:0x10f}

def signature(st):return tuple((b['offset'],b['taken']) for b in st.branches)

def annotate_liveness(st):
    # Exact register-byte/status-flag liveness for the executed path, with every
    # actual memory read/write and ABI argument as an observation. Memory alias
    # def/use remains in byte_definitions; no alias edge is guessed away here.
    # Void return: callee-saved GPRs and ESP are contractual outputs.
    live={f'{r}.{k}' for r in ['ebx','esi','edi','ebp','esp'] for k in range(4)}
    for step in reversed(st.trace):
        step['lane_liveness_after']=sorted(live)
        for w in step['writes']:
            cells=[f"{w['register']}.{k}" for k in w['bytes']] if w['kind']=='register' else ([w['flag']] if w['kind']=='flag' else [])
            w['live_defined_lanes']=[x for x in cells if x in live]
            w['dead_defined_lanes']=[x for x in cells if x not in live]
            live.difference_update(cells)
        for r in step['reads']:
            if 'register' in r:live.update(f"{r['register']}.{k}" for k in r['bytes'])
            elif r['kind']=='flag':live.add(r['flag'])
        step['lane_liveness_before']=sorted(live)

def clean_step(step):return {k:v for k,v in step.items() if not k.startswith('_')}

def compare(nat,can,verbose=True):
    ncfg=cfg(nat);ccfg=cfg(can)
    skeleton=lambda g:[(b['start'],b['successors'],b['reachable']) for b in g['blocks']]
    need(skeleton(ncfg)==skeleton(ccfg),'different CFG; this mapping must be re-audited')
    np=execute(nat);cp=execute(can);cd={signature(p):p for p in cp};need(len(cd)==len(cp),'duplicate path signatures')
    need({signature(p) for p in np}==set(cd),'different feasible control paths')
    mapping={r['offset']:MAP.get(r['offset'],r['offset']) for r in nat['rows']}
    need(set(mapping.values())==set(can['by_offset']),'mapping is not a complete bijection')
    relocations=[]
    for p,q in mapping.items():
        rr=nat['by_offset'][p]['relocations'];ss=can['by_offset'][q]['relocations']
        canon=lambda items:[(r['type'],r['symbol'],r.get('target_section'),r.get('string_hex'),None if 'string_hex' in r else r['addend']) for r in items]
        if rr or ss:relocations.append({'native_instruction':p,'candidate_instruction':q,'equal':canon(rr)==canon(ss),'native':rr,'candidate':ss})
    paths=[];overall=all(r['equal'] for r in relocations)
    for n in np:
        c=cd[signature(n)];conditions=n.constraints+c.constraints;label='path'+str(len(paths))
        domain_equal=prove(label+':complete-path-domain',z.And(*n.constraints),z.And(*c.constraints),[])
        overall &= domain_equal
        overlap=feasible(conditions)
        if not overlap:
            # An unequal domain is a measured difference, even where no input
            # selects the same textual arm in both implementations. Do not
            # compare instructions under a contradictory conjunction.
            need(not domain_equal,'equal nonempty domains cannot have empty overlap')
            paths.append({'id':label,'branches':n.branches,'complete_path_domain_equal':False,'paired_domain_overlap':False,'constraints':[NODES.add(x) for x in conditions],'executed_native_instructions':len(n.trace),'executed_candidate_instructions':len(c.trace),'instruction_checks':[],'observations':[],'native_trace':[clean_step(s) for s in n.trace],'candidate_trace':[clean_step(s) for s in c.trace]})
            continue
        nt={s['offset']:s for s in n.trace};ct={s['offset']:s for s in c.trace}
        need({mapping[p] for p in nt}==set(ct),'dynamic instruction mapping does not cover path')
        per_instruction=[]
        for p,a in nt.items():
            q=mapping[p];b=ct[q];checks=[]
            for kind in ['_outputs','_loads','_stores']:
                aa=a[kind];bb=b[kind];need(len(aa)==len(bb),f'output shape differs {p:x}->{q:x} {kind}')
                for j,(av,bv) in enumerate(zip(aa,bb)):
                    if kind=='_outputs':
                        ad=a['_output_descriptors'][j];bd=b['_output_descriptors'][j];need(ad[2]==bd[2],'register write width mismatch')
                        alive={x for x in a['lane_liveness_after']};blive={x for x in b['lane_liveness_after']}
                        # Union retains every constrained lane in either range.
                        used=[k for k in range(ad[2]) if f'{ad[0]}.{ad[1]+k}' in alive or f'{bd[0]}.{bd[1]+k}' in blive]
                        for k in used:checks.append(prove(f'{label}:instruction:{p:03x}->{q:03x}:live-output:{j}:byte{k}',z.Extract(8*k+7,8*k,av),z.Extract(8*k+7,8*k,bv),conditions))
                        continue
                    for component,(x,y) in enumerate(zip(av,bv) if isinstance(av,tuple) else [(av,bv)]):
                        checks.append(prove(f'{label}:instruction:{p:03x}->{q:03x}:{kind}:{j}:{component}',x,y,conditions))
            if '_condition' in a:checks.append(prove(f'{label}:predicate:{p:03x}',a['_condition'],b['_condition'],conditions))
            for f in set(a['_flag_outputs'])|set(b['_flag_outputs']):
                if f in a['lane_liveness_after'] or f in b['lane_liveness_after']:
                    need(f in a['_flag_outputs'] and f in b['_flag_outputs'],'live flag definition missing')
                    checks.append(prove(f'{label}:instruction:{p:03x}->{q:03x}:live-flag:{f}',a['_flag_outputs'][f],b['_flag_outputs'][f],conditions))
            per_instruction.append({'native':p,'candidate':q,'checks':len(checks),'equal':all(checks)})
            overall &= all(checks)
        need(len(n.calls)==len(c.calls),'different call count')
        observations=[]
        for k,(a,b) in enumerate(zip(n.calls,c.calls)):
            need(a['target']==b['target'],'different call target')
            checks=[prove(f'{label}:call:{k}:whole-memory',a['memory'],b['memory'],conditions)]
            need(set(a['stack'])==set(b['stack']),'different stack-cell inventory')
            checks.extend(prove(f'{label}:call:{k}:stack-byte:{p:x}',a['stack'][p],b['stack'][p],conditions) for p in a['stack'])
            checks.extend(prove(f'{label}:call:{k}:argument:{j}',x,y,conditions) for j,(x,y) in enumerate(zip(a['args'],b['args'])))
            observations.append({'kind':'call','target':a['target'],'arguments':[NODES.add(x) for x in a['args']],'whole_memory_equal':checks[0],'all_equal':all(checks),'native_gprs':{r:NODES.add(v) for r,v in a['registers'].items()},'candidate_gprs':{r:NODES.add(v) for r,v in b['registers'].items()}})
            overall &= all(checks)
        retchecks=[prove(label+':return:whole-memory',n.mem,c.mem,conditions)]
        need(set(n.stack)==set(c.stack),'different return stack-cell inventory')
        retchecks.extend(prove(f'{label}:return:stack-byte:{p:x}',n.stack[p],c.stack[p],conditions) for p in n.stack)
        retchecks.extend(prove(label+':return:gpr:'+r,n.reg[r],c.reg[r],conditions) for r in ['ebx','esi','edi','ebp','esp'])
        retchecks.extend(prove(label+':return:flag:'+f,n.flag[f],c.flag[f],conditions) for f in FLAGS)
        extra=[prove(label+':return:extra-caller-gpr:'+r,n.reg[r],c.reg[r],conditions) for r in ['eax','ecx','edx']]
        observations.append({'kind':'return','all_equal':all(retchecks),'whole_memory_equal':retchecks[0],'abi_gpr_checks':5,'extra_caller_gpr_checks':3,'extra_caller_gprs_equal':all(extra),'status_flag_checks':6});overall &= all(retchecks)
        paths.append({'id':label,'branches':n.branches,'complete_path_domain_equal':domain_equal,'paired_domain_overlap':True,'constraints':[NODES.add(x) for x in conditions],'executed_native_instructions':len(n.trace),'executed_candidate_instructions':len(c.trace),'instruction_checks':per_instruction,'observations':observations,'native_trace':[clean_step(s) for s in n.trace],'candidate_trace':[clean_step(s) for s in c.trace]})
    rows=[]
    for a in nat['rows']:
        b=can['by_offset'][mapping[a['offset']]]
        rows.append({'native':{k:v for k,v in a.items() if k!='instruction'},'candidate':{k:v for k,v in b.items() if k!='instruction'},'executed':a['offset'] in ncfg['reachable_instruction_offsets'],'mapping_basis':'consumer/value match' if a['offset'] in MAP else 'same relative offset; semantic outputs independently compared'})
    return {'status':'all admitted relational obligations discharged' if overall else 'difference or unknown','equal':bool(overall),'native':{k:v for k,v in nat.items() if k not in ['rows','by_offset']},'candidate':{k:v for k,v in can.items() if k not in ['rows','by_offset']},'cfg':ncfg,'mapping':rows,'relocation_checks':relocations,'paths':paths,'denominators':{'bytes_each':nat['size'],'decoded_instructions_each':len(nat['rows']),'reachable_instructions_each':len(ncfg['reachable_instruction_offsets']),'unreachable_padding_instructions_each':len(ncfg['unreachable_instruction_offsets']),'blocks_each':len(ncfg['blocks']),'reachable_blocks_each':sum(b['reachable'] for b in ncfg['blocks']),'edges_each':len(ncfg['edges']),'reachable_edges_each':len(ncfg['reachable_edges']),'feasible_paths_each':len(np),'mapped_instructions':len(rows),'compared_dynamic_instruction_pairs':sum(len(p['instruction_checks']) for p in paths),'paired_call_boundaries':sum(max(0,len(p['observations'])-1) for p in paths),'paired_returns':sum(bool(p['observations']) for p in paths)}}

def main():
    mode=sys.argv[1] if len(sys.argv)>1 else 'audit'
    if mode=='audit':
        result=compare(elf_function(HERE/'native.o'),elf_function(HERE/'candidate.o'))
        result['proofs']=PROOFS;result['expression_dag']=NODES.nodes
        result['apparatus']={'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'python':sys.version,'z3':z.get_version_string(),'capstone':cs.__version__,'feasibility_cache':FEASIBILITY_STATS}
        (HERE/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'status':result['status'],'denominators':result['denominators'],'proof_results':{k:sum(p['result']==k for p in PROOFS) for k in sorted(set(p['result'] for p in PROOFS))},'expression_dag_nodes':len(NODES.nodes)},indent=2))
        return 0 if result['equal'] else 1
    raise RuntimeError('unknown mode')

if __name__=='__main__':sys.exit(main())
