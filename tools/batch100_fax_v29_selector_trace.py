#!/usr/bin/env python3
"""Read-only ordered selector and ordinary ABI CFG evidence; not an EXACT verdict."""
import json,io,re,hashlib
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from toolchain import jumptable as j
NAME='RxNextStateV29'
CAND=d.ROOT/'build/batch100-fax-v29-next-flags/V29r_prc/byte-transition-flags/candidate.o'
def trace(path):
 e=ELFFile(io.BytesIO(Path(path).read_bytes()));sections=list(e.iter_sections());s=e.get_section_by_name('.symtab')
 owners=[x for x in s.iter_symbols() if x.name==NAME and x['st_info']['type']=='STT_FUNC' and x['st_size']];assert len(owners)==1
 owner=owners[0];start=owner['st_value'];end=start+owner['st_size'];sec=owner['st_shndx'];rows=j.instructions(str(path),sections[sec].name,start,end);by={x[0]:i for i,x in enumerate(rows)}
 assert owner['st_size']==467
 indirect=[i for i,r in enumerate(rows) if r[2].startswith(('j','call')) and r[3].startswith('*')];assert len(indirect)==1
 i=indirect[0];cmp,guard,jump=rows[i-2:i+1];assert cmp[2:] == ('cmp','$0x4,%eax') and guard[2]=='ja' and jump[2]=='jmp'
 protected=set(range(cmp[0]+1,jump[0]+len(jump[1])))
 rels=[]
 for rs in sections:
  if isinstance(rs,RelocationSection):
   assert not rs.is_RELA()
   syms=sections[rs['sh_link']]
   rels.extend((rs['sh_info'],r,syms.get_symbol(r['r_info_sym'])) for r in rs.iter_relocations())
 own=[(r,sym) for sn,r,sym in rels if sn==sec and start<=r['r_offset']<end];assert len(own)==25
 dispatch=[(r,sym) for r,sym in own if jump[0]<=r['r_offset']<jump[0]+len(jump[1])];assert len(dispatch)==1
 r,sym=dispatch[0];assert r['r_info_type']==1 and sym['st_info']['type']=='STT_SECTION' and r['r_offset']==jump[0]+len(jump[1])-4
 tableid=sym['st_shndx'];table=sections[tableid];tableoff=int.from_bytes(jump[1][-4:],'little')+sym['st_value'];assert table['sh_flags']==2 and tableoff%4==0
 entries=sorted([(r,sym)for sn,r,sym in rels if sn==tableid and tableoff<=r['r_offset']<tableoff+20],key=lambda x:x[0]['r_offset']);assert len(entries)==5
 targets=[]
 for n,(r,sym) in enumerate(entries):
  assert r['r_offset']==tableoff+4*n and r['r_info_type']==1 and sym['st_info']['type']=='STT_SECTION' and sym['st_shndx']==sec
  target=int.from_bytes(table.data()[r['r_offset']:r['r_offset']+4],'little')+sym['st_value'];assert target in by and target not in protected;targets.append(target)
 assert [x-start for x in targets]==[106,151,207,269,321]
 for sn,r,sym in rels:
  if sym['st_shndx']==sec and r['r_info_type'] in (1,2):
   target=int.from_bytes(sections[sn].data()[r['r_offset']:r['r_offset']+4],'little',signed=True)+sym['st_value'];assert target not in protected and (r['r_info_type']!=2 or target+4 not in protected)
 assert rows[0][2:]==('push','%ebx') and rows[1][2:]==('sub','$0x8,%esp')
 pending=[(start,0,False)];seen={};returns=0;calls=[];outargs=[]
 allowed={'push','pop','sub','add','addl','mov','movl','movw','movb','movswl','movzwl','movzbl','and','andb','or','orb','cmp','cmpl','sbb','not','call','nop','lea','ret'}
 while pending:
  addr,depth,saved=pending.pop()
  if addr in seen:assert seen[addr]==(depth,saved);continue
  seen[addr]=(depth,saved);n=by[addr];_,raw,m,op=rows[n];assert m in allowed or m.startswith('j')
  if m=='push':assert n==0 and depth==0 and op=='%ebx';depth=4;saved=True
  elif m=='pop':assert depth==4 and saved and op=='%ebx';depth=0;saved=False
  elif '%esp' in op:
   if op=='$0x8,%esp':
    assert (m=='sub' and n==1 and depth==4) or (m=='add' and depth==12);depth += 8 if m=='sub' else -8
   elif op.endswith('(%esp)'):
    assert depth==12 and m in ('mov','movl');match=re.search(r',(?:(0x[0-9a-f]+))?\(%esp\)$',op);assert match and int(match[1]or'0',16) in (0,4);outargs.append(addr-start)
   else:assert m=='mov' and op=='0x10(%esp),%ebx' and depth==12 and n==2
  elif re.search(r'(?:^|,)%(?:ebx|bx|bl|bh|edi|di|esi|si|ebp|bp)$',op):
   assert m in ('cmp','test','lea') and (m!='lea' or op=='0x0(%esi,%eiz,1),%esi')
  if m=='call':
   assert depth==12 and saved and raw[0]==0xe8 and len(raw)==5
   rr=[(r,sym) for r,sym in own if addr<r['r_offset']<addr+5];assert len(rr)==1
   r,sym=rr[0];assert r['r_info_type']==2 and r['r_offset']==addr+1 and int.from_bytes(raw[1:],'little',signed=True)==-4
   assert sym['st_info']['type'] in ('STT_FUNC','STT_NOTYPE') and sym.name in ('dsplibs_debug_printf','FPM_AGC_Freeze');calls.append((addr-start,sym.name))
  if m=='ret':assert depth==0 and not saved and not op;returns+=1;continue
  if m=='jmp' and op.startswith('*'):successors=targets
  elif m.startswith('j'):
   match=re.match(r'^([0-9a-f]+)\s',op);assert match;target=int(match[1],16);assert target in by and target not in protected;successors=[target]
   if m!='jmp':successors.append(rows[n+1][0])
  else:assert n+1<len(rows);successors=[rows[n+1][0]]
  pending.extend((x,depth,saved)for x in successors)
 assert returns==5 and len(calls)==7
 try:j.prove(path,NAME);refusal=None
 except j.Refused as ex:refusal=str(ex)
 return {'sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest(),'size':467,'selector_count':5,'table_section':table.name,'table_offset':tableoff,'ordered_destinations':[x-start for x in targets],'stack_bytes':12,'outgoing_bytes':8,'saved_register':'ebx','reachable_instructions':len(seen),'returns':returns,'calls':calls,'outgoing_stores':outargs,'owner_relocations':25,'generic_prover_refusal':refusal}
report={k:trace(p)for k,p in [('blob',d.b.BLOB),('candidate',CAND)]}
a,ar=d.b.body(d.b.BLOB,NAME);z,zr=d.b.body(CAND,NAME);assert ar.keys()==zr.keys()
for offset in ar:
 if offset!=23:assert ar[offset]==zr[offset]
 aa=bytearray(a);zz=bytearray(z)
for offset in ar:aa[offset:offset+4]=b'\0'*4;zz[offset:offset+4]=b'\0'*4
assert aa==zz
report['masked_bytes_equal']=True;report['other_24_relocations_equal']=True;report['verdict']='UNRESOLVED; independent evidence only'
out=d.ROOT/'build/batch100-fax-v29-selector-trace.json';out.write_text(json.dumps(report,indent=2)+'\n');print('Selector trace: 2 objects; five ordered destinations agree, 25 relocation fields, 5 balanced ABI exits; shared prover still refuses, no EXACT gain claimed')
