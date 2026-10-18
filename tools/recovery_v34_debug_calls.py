#!/usr/bin/env python3
"""Bounded backwards CFG resolution of V34 diagnostic format arguments.

Tracks cdecl outgoing stack slots, register copies and relocatable literals.
Reports unresolved sites rather than treating a nearby string as the format.
"""
import json,collections
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/v34-root-closure293'
def audit(path):
 import recovery_v34_handshake_boundary as inv
 from capstone import Cs,CS_ARCH_X86,CS_MODE_32
 from capstone.x86 import X86_OP_IMM,X86_OP_REG,X86_OP_MEM
 from recovery_fax_debug_audit import Object
 o=Object(path);textindex=next(i for i,(n,d) in o.sections.items() if n=='.text');data=o.sections[textindex][1];fn=next(s for s in o.functions if s.name=='v34handshak');md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True;ins=list(md.disasm(bytes(data[fn['st_value']:fn['st_value']+fn['st_size']]),fn['st_value']));idx={i.address:k for k,i in enumerate(ins)};rel={at:(symbol,kind) for sec,at,symbol,kind in o.relocations if sec==textindex};pred=collections.defaultdict(list)
 for k,i in enumerate(ins):
  if i.mnemonic=='ret':continue
  if i.mnemonic.startswith('j'):
   if i.operands[0].type==X86_OP_IMM and i.operands[0].imm in idx:pred[idx[i.operands[0].imm]].append(k)
   if i.mnemonic=='jmp':continue
  if k+1<len(ins):pred[k+1].append(k)
 indirect_entries=set()
 for sec,at,symbol,kind in o.relocations:
  if sec==textindex or kind!=1 or symbol['st_shndx']!=textindex:continue
  target=symbol['st_value']+int.from_bytes(o.sections[sec][1][at:at+4],'little',signed=True)
  if target in idx:indirect_entries.add(idx[target])
 def regbase(name):
  return {'ax':'eax','al':'eax','ah':'eax','bx':'ebx','bl':'ebx','bh':'ebx','cx':'ecx','cl':'ecx','ch':'ecx','dx':'edx','dl':'edx','dh':'edx','sp':'esp','bp':'ebp','si':'esi','di':'edi'}.get(name,name)
 def location(i,operand):
  if operand.type==X86_OP_REG:return ('reg',i.reg_name(operand.reg))
  if operand.type==X86_OP_MEM and i.reg_name(operand.mem.base)=='esp' and not operand.mem.index:return ('stack',operand.mem.disp)
 def literal(i,operand):
  memory=operand.type==X86_OP_MEM and not operand.mem.base and not operand.mem.index
  if operand.type!=X86_OP_IMM and not memory:return None
  rows=[(at,s,k) for at,(s,k) in rel.items() if i.address<=at<i.address+i.size]
  for at,s,kind in rows:
   if kind!=1:continue
   add=int.from_bytes(data[at:at+4],'little',signed=True);payload=o.payload(s,add)
   if memory and isinstance(s['st_shndx'],int):
    target_at=s['st_value']+add
    pointer=next(((sym,typ) for sec,pos,sym,typ in o.relocations if sec==s['st_shndx'] and pos==target_at),None)
    if pointer and pointer[1]==1:
     secdata=o.sections[s['st_shndx']][1];offset=int.from_bytes(secdata[target_at:target_at+4],'little',signed=True);pointed=o.payload(pointer[0],offset)
     if pointed[0]=='string':return ('string',pointed[1].decode('ascii'))
   if not memory and payload[0]=='string':return ('string',payload[1].decode('ascii'))
  return None if memory else ('integer',operand.imm)
 cache={}
 def resolve(k,loc,seen=frozenset(),depth=0):
  if depth>120:return {('unknown','depth')}
  key=(k,loc)
  if key in cache:return cache[key]
  if key in seen:return {('unknown','cycle')}
  seen=seen|{key};found={('unknown','omitted indirect predecessor')} if k in indirect_entries else set()
  for prev in pred[k]:
   i=ins[prev];ops=i.operands
   destination=location(i,ops[0]) if ops else None
   writes=bool(ops) and i.mnemonic not in ('cmp','test','push','call') and not i.mnemonic.startswith('j')
   espchange=i.mnemonic in ('push','pop','enter','leave') or (writes and destination==('reg','esp'))
   aliaswrite=writes and loc[0]=='reg' and destination and destination[0]=='reg' and regbase(destination[1])==regbase(loc[1]) and destination!=loc
   if loc[0]=='stack' and espchange:found.add(('unknown','ESP adjustment'));continue
   if aliaswrite:found.add(('unknown','partial-register alias write'));continue
   if writes and destination==loc:
    if i.mnemonic=='mov' and len(ops)==2:
     value=literal(i,ops[1]);src=location(i,ops[1])
     if value is not None:found.add(value)
     elif src is not None:found|=resolve(prev,src,seen,depth+1)
     else:found.add(('unknown',i.mnemonic+' '+i.op_str))
    else:found.add(('unknown',i.mnemonic+' '+i.op_str))
   elif i.mnemonic=='call' and loc[0]=='reg' and regbase(loc[1]) in ('eax','ecx','edx'):found.add(('unknown','call clobber'))
   else:found|=resolve(prev,loc,seen,depth+1)
  cache[key]=found or {('unknown','entry')}
  return cache[key]
 calls=inv.inspect(path)['calls'];rows=[]
 for c in calls:
  if c['op']!='call' or c['target']!='dsplibs_debug_printf':continue
  k=idx[c['at']];args={str(slot):sorted(resolve(k,('stack',slot))) for slot in range(0,28,4)}
  import re
  arity=max(len(re.findall(r'%(?!%)[-+ #0-9.*hlLzjt]*[A-Za-z]',v[1])) for v in args['0'] if v[0]=='string') if all(v[0]=='string' for v in args['0']) else 6
  args={k:v for k,v in args.items() if int(k)<=4*arity}
  rows.append({'at':hex(c['at']),'arguments':args,'resolved_format':all(v[0]=='string' for v in args['0'])})
 return {'path':str(path),'sha256':inv.sha(path),'calls':rows,'resolved':sum(r['resolved_format'] for r in rows),'denominator':len(rows)}
def main():
 import argparse
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--out',type=Path,default=OUT,help='artifact directory containing the shared candidate')
 parser.add_argument('--reference',type=Path,default=ROOT/'ref/slmodemd/dsplibs.o')
 args=parser.parse_args()
 import recovery_v34_handshake_boundary as inv
 out=args.out.resolve()
 report={n:audit(p) for n,p in [('native',args.reference.resolve()),('candidate',out/'hi-shared-table2/V34hshak.o')]}
 from elftools.elf.elffile import ELFFile
 source=out/'hi-shared-table2/V34hshak.o';raw=bytearray(source.read_bytes());known=next(r['arguments']['0'][0][1] for r in report['candidate']['calls'] if len(r['arguments']['0'])==1)
 with source.open('rb') as stream:
  elf=ELFFile(stream)
  for section in elf.iter_sections():
   if not section.name.startswith('.rodata.str'):continue
   offset=section.data().find(known.encode()+b'\0')
   if offset>=0:raw[section['sh_offset']+offset]^=0x20;break
  else:raise AssertionError('known referenced format not found')
 wrong=out/'known-wrong-format.o';wrong.write_bytes(raw);negative=audit(wrong)
 def formats(result):return collections.Counter(tuple(tuple(v) for v in row['arguments']['0']) for row in result['calls'])
 assert negative['denominator']==report['candidate']['denominator'] and formats(negative)!=formats(report['candidate'])
 report['negative_control']={'known_format':known,'modified_object':str(wrong),'sha256':inv.sha(wrong),'calls_preserved':negative['denominator'],'wrong_format_rejected':True}
 (out/'debug-call-arguments.json').write_text(json.dumps(report,indent=2)+'\n')
 for name in ('native','candidate'):
  r=report[name];print(name,r['resolved'],'/',r['denominator'])
 print('wrong-format negative rejected, unchanged call denominator',negative['denominator'])

if __name__ == "__main__":
 main()
