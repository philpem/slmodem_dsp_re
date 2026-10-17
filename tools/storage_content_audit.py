#!/usr/bin/env python3
"""Triage initialized allocated object bytes and direct numeric-pool consumers.

Relocation fields are masked only while their kind/offset/target remains a
separate comparison. Duplicate names are not collapsed: unresolved pairs remain
ambiguous. Byte shifts/containment nominate extent or address review, never an
automatic source repair. .rodata.cst consumer multisets are not path/guard,
inline, value-width conversion or whole-object behavioral proofs.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import sys
_saved_path=sys.path[:]
sys.path[:]=[p for p in sys.path if Path(p or ".").resolve()!=Path(__file__).resolve().parent]
import dis
sys.path[:]=_saved_path
from elftools.elf.elffile import ELFFile
import storage_layout_audit as layout


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inventory(path):
    with Path(path).open('rb') as stream:
        elf=ELFFile(stream)
        assert elf.elfclass==32 and elf.little_endian and elf['e_machine']=='EM_386' and elf['e_type']=='ET_REL'
        sections=list(elf.iter_sections()); tab=elf.get_section_by_name('.symtab')
        symbols=list(tab.iter_symbols()); file=None; records=[]; functions=[]
        by_section=defaultdict(list); errors=[]
        for i,s in enumerate(symbols):
            if s['st_info']['type']=='STT_FILE':file=s.name
            index=s['st_shndx']
            if not isinstance(index,int):continue
            sec=sections[index]
            if s['st_info']['type']=='STT_FUNC' and s['st_size']:
                functions.append((index,s['st_value'],s['st_value']+s['st_size'],s.name,i))
            if s['st_info']['type']!='STT_OBJECT' or not sec['sh_flags']&2 or sec['sh_flags']&4:continue
            row={'name':s.name,'binding':s['st_info']['bind'],'file':file if s['st_info']['bind']=='STB_LOCAL' else None,
                 'symbol_index':i,'section':sec.name,'section_index':index,'offset':s['st_value'],'size':s['st_size'],
                 'kind':sec['sh_type'],'relocations':[]}
            if row['offset']+row['size']>sec['sh_size']:
                errors.append({'kind':'object_outside_section','record':row});continue
            row['bytes']=bytearray(sec.data()[row['offset']:row['offset']+row['size']]) if sec['sh_type']!='SHT_NOBITS' else None
            records.append(row);by_section[index].append(row)
        named_counts=Counter((r['name'],r['binding']) for r in records)
        by_symbol={r['symbol_index']:r for r in records}
        def object_target(row,relative):
            if named_counts[(row['name'],row['binding'])]==1:
                return ['object',row['name'],relative]
            peers=[r for r in records if (r['name'],r['binding'],r['file'])==(row['name'],row['binding'],row['file'])]
            if len(peers)==1:return ['local-object',row['name'],row['file'],relative]
            errors.append({'kind':'ambiguous_local_relocation_target','symbol_index':row['symbol_index']})
            return ['ambiguous-local-object',row['name'],row['file'],row['section'],row['offset'],relative]
        def target(s,addend,symbol_index):
            index=s['st_shndx']
            if not isinstance(index,int):return ['symbol',s.name,addend]
            sec=sections[index]; at=s['st_value']+addend
            if sec.name.startswith('.rodata.cst'):
                width=sec['sh_entsize'] or int(sec.name.rsplit('cst',1)[1])
                if at<0 or at+width>sec['sh_size']:raise ValueError('constant pool target outside section')
                return ['numeric-pool',width,sec.data()[at:at+width].hex()]
            if sec.name.startswith('.rodata.str'):
                raw=sec.data();return ['string',raw[at:raw.index(b'\0',at)].hex()]
            if s['st_info']['type']=='STT_OBJECT' and symbol_index in by_symbol:return object_target(by_symbol[symbol_index],addend)
            if s['st_info']['type']=='STT_FUNC':return ['function',s.name,addend]
            if sec['sh_flags']&4:
                owners=[r for r in functions if r[0]==index and r[1]<=at<r[2]]
                if len(owners)==1:return ['function',owners[0][3],at-owners[0][1]]
            owners=[r for r in by_section[index] if r['offset']<=at<r['offset']+r['size']]
            if len(owners)==1:return object_target(owners[0],at-owners[0]['offset'])
            return ['section-unresolved',sec.name,at]
        pools=defaultdict(Counter); pool_locations=[]; relocations=0; unsupported=0
        for relsec in sections:
            if relsec['sh_type'] not in ('SHT_REL','SHT_RELA'):continue
            if relsec['sh_type']=='SHT_RELA':
                errors.append({'kind':'unsupported_RELA','section':relsec.name});continue
            owner_index=relsec['sh_info']; sec=sections[owner_index]
            if not sec['sh_flags']&2:continue
            raw=sec.data(); symbols_table=sections[relsec['sh_link']]
            for rel in relsec.iter_relocations():
                relocations+=1; off=rel['r_offset']; kind=rel['r_info_type']
                owners=[r for r in by_section[owner_index] if r['offset']<=off<r['offset']+r['size']]
                if kind not in (1,2):
                    unsupported+=1;errors.append({'kind':'unsupported_relocation','section':sec.name,'offset':off,'type':kind});continue
                try:
                    assert off+4<=len(raw)
                    addend=struct.unpack_from('<i',raw,off)[0]
                    dest=target(symbols_table.get_symbol(rel['r_info_sym']),addend,rel['r_info_sym'])
                except (ValueError,AssertionError,struct.error) as exc:
                    errors.append({'kind':'bad_relocation','section':sec.name,'offset':off,'error':str(exc)});continue
                for row in owners:
                    relative=off-row['offset']
                    if relative+4>row['size']:
                        errors.append({'kind':'relocation_crosses_object','symbol_index':row['symbol_index'],'offset':off});continue
                    row['relocations'].append([relative,kind,dest]);row['bytes'][relative:relative+4]=b'\0'*4
                if dest[0]=='numeric-pool' and sec['sh_flags']&4 and kind==1:
                    funcs=[r for r in functions if r[0]==owner_index and r[1]<=off<r[2]]
                    if len(funcs)==1:
                        pools[funcs[0][3]][(dest[1],dest[2])]+=1
                        pool_locations.append({'function':funcs[0][3],'relocation_offset':off,'width':dest[1],'payload':dest[2]})
                    else:errors.append({'kind':'pool_consumer_owner_ambiguous','offset':off})
        return {'path':str(path),'sha256':digest(Path(path).read_bytes()),'records':records,'pools':dict(pools),
                'pool_locations':pool_locations,'errors':errors,'allocated_relocations':relocations,'unsupported':unsupported}


def summary(row):
    return {k:v for k,v in row.items() if k not in ('bytes','section_index')}


def payload_compare(a,b):
    left,right=bytes(a['bytes']),bytes(b['bytes'])
    count=min(len(left),len(right)); changed=[i for i in range(count) if left[i]!=right[i]]
    result={'reference':summary(a),'candidate':summary(b),'byte_denominator':count,
            'masked_bytes_equal':left==right,'relocations_equal':a['relocations']==b['relocations'],
            'reference_payload_sha256':digest(left),'candidate_payload_sha256':digest(right),
            'different_aligned_bytes':len(changed),'first_differences':[{'offset':i,'reference':left[max(0,i-4):i+5].hex(),'candidate':right[max(0,i-4):i+5].hex()} for i in changed[:4]]}
    if not a['relocations'] and not b['relocations'] and left and right:
        result['reference_contained_at']=right.find(left)
        result['candidate_contained_at']=left.find(right)
        shifts=[]
        for delta in range(-64,65):
            if not delta:continue
            x,y=max(0,delta),max(0,-delta);n=min(len(left)-x,len(right)-y)
            if n>=16 and len(set(left[x:x+n]))>2 and left[x:x+n]==right[y:y+n]:
                shifts.append({'reference_offset':x,'candidate_offset':y,'overlap_bytes':n})
        result['exact_shifted_overlaps']=sorted(shifts,key=lambda r:-r['overlap_bytes'])[:4]
    return result


def compare(ref,cand):
    groups=[]
    for inv in (ref,cand):
        g=defaultdict(list)
        for r in inv['records']:
            if r['kind']!='SHT_NOBITS':g[(r['name'],r['binding'])].append(r)
        groups.append(g)
    rows=[]; unmatched=[]; ambiguous=[]; compared=0; equal=0
    for key in sorted(groups[0].keys()|groups[1].keys()):
        left,right=groups[0].get(key,[]),groups[1].get(key,[])
        pairs=[]
        if len(left)==len(right)==1:pairs=[(left[0],right[0])]
        elif left and right:
            # FILE is only used to disambiguate duplicate LOCAL names; no
            # nearest-FILE ownership inference is made for globals.
            for owner in {r['file'] for r in left+right}:
                aa=[r for r in left if r['file']==owner];bb=[r for r in right if r['file']==owner]
                if len(aa)==len(bb)==1:pairs.append((aa[0],bb[0]))
            used_a={a['symbol_index'] for a,b in pairs};used_b={b['symbol_index'] for a,b in pairs}
            pending_a=[r for r in left if r['symbol_index'] not in used_a];pending_b=[r for r in right if r['symbol_index'] not in used_b]
            if pending_a or pending_b:ambiguous.append({'key':key,'reference':[summary(r) for r in pending_a],'candidate':[summary(r) for r in pending_b]})
        else:unmatched.append({'key':key,'reference':[summary(r) for r in left],'candidate':[summary(r) for r in right]})
        for a,b in pairs:
            compared+=1;row=payload_compare(a,b)
            if row['masked_bytes_equal'] and row['relocations_equal']:equal+=1
            else:rows.append(row)
    pool_rows=[];pool_equal=0;pool_compared=0
    for name in sorted(ref['pools'].keys()&cand['pools'].keys()):
        pool_compared+=1;a,b=ref['pools'][name],cand['pools'][name]
        if a==b:pool_equal+=1
        else:pool_rows.append({'function':name,'reference_only':list((a-b).elements()),'candidate_only':list((b-a).elements()),'reference_consumers':sum(a.values()),'candidate_consumers':sum(b.values())})
    return {'matched_initialized_objects':compared,'equal_objects':equal,'different_objects':len(rows),'rows':rows,
            'unmatched_name_groups':unmatched,'ambiguous_name_groups':ambiguous,
            'bounded_shift_radius_bytes':64,'minimum_shift_overlap_bytes':16,'numeric_pool_shared_functions':pool_compared,'numeric_pool_equal_functions':pool_equal,'numeric_pool_differences':pool_rows,
            'numeric_pool_reference_only_functions':sorted(ref['pools'].keys()-cand['pools'].keys()),
            'numeric_pool_candidate_only_functions':sorted(cand['pools'].keys()-ref['pools'].keys())}


def controls():
    with tempfile.TemporaryDirectory(prefix='storage-content-') as temp:
        root=Path(temp)
        def build(name,lead=0,values=range(1,9),target='one',pool='0x3f800000'):
            p=root/(name+'.s');o=root/(name+'.o')
            p.write_text('.data\n.fill %d,1,0\n.globl table\n.type table,@object\ntable:\n'%lead+'.long '+','.join(map(str,values))+'\n.size table,.-table\n.globl pointer\n.type pointer,@object\npointer:\n.long '+target+'\n.size pointer,4\n.text\n.globl consume\n.type consume,@function\nconsume:\n flds .LC0\n ret\n.size consume,.-consume\n.section .rodata.cst4,"aM",@progbits,4\n.LC0:\n.long '+pool+'\n')
            subprocess.run(['as','--32','-o',str(o),str(p)],check=True)
            return inventory(o)
        base=build('base');shift=build('shift',lead=12);mutation=build('mutation',values=[1,2,3,99,5,6,7,8]);inside=build('inside',values=range(0,8));reloc=build('reloc',target='two')
        assert compare(base,base)['equal_objects']==2
        assert compare(base,shift)['equal_objects']==2 and base['records'][0]['offset']!=shift['records'][0]['offset']
        assert compare(base,mutation)['different_objects']==1
        assert compare(base,inside)['rows'][0]['exact_shifted_overlaps']
        row=compare(base,reloc)['rows'][0];assert row['masked_bytes_equal'] and not row['relocations_equal']
        assert compare(base,shift)['numeric_pool_equal_functions']==1
        changed_pool=build('changed-pool',pool='0x40000000')
        assert len(compare(base,changed_pool)['numeric_pool_differences'])==1
    return 7


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('reference',type=Path);ap.add_argument('candidate',type=Path);ap.add_argument('--json',type=Path,required=True)
    args=ap.parse_args();control_count=controls();a,b=inventory(args.reference),inventory(args.candidate);report=compare(a,b)
    storage_a,storage_b=layout.inventory(args.reference),layout.inventory(args.candidate)
    report.update({'reference':{k:v for k,v in a.items() if k not in ('records','pools','pool_locations')},'candidate':{k:v for k,v in b.items() if k not in ('records','pools','pool_locations')},
                   'controls':control_count,'storage_layout_comparison':layout.compare(storage_a,storage_b),
                   'limits':'Initialized STT_OBJECT allocated noncode payload only; NOBITS handled by storage-layout inventory. Sliding overlaps bounded to +/-64 bytes, minimum16 bytes and morethan2 distinctbytevalues; full containment searches whole paired symbol. Strings not normalized except relocation destinations. Unnamed numeric pools only direct R_386_32 text consumers. Multisets cannot prove guard/order/multiplicity equivalence under different inlining. Ambiguities/errors remain visible; shifts are nominations, not fixes.'})
    args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print('%d controls; initialized objects %d matched, %d equal, %d different; %d unmatched name groups, %d ambiguous groups; errors %d/%d' % (control_count,report['matched_initialized_objects'],report['equal_objects'],report['different_objects'],len(report['unmatched_name_groups']),len(report['ambiguous_name_groups']),len(a['errors']),len(b['errors'])))
    print('numeric pool consumers: %d shared functions, %d equal, %d different; unmatched functions %d/%d' % (report['numeric_pool_shared_functions'],report['numeric_pool_equal_functions'],len(report['numeric_pool_differences']),len(report['numeric_pool_reference_only_functions']),len(report['numeric_pool_candidate_only_functions'])))
    for row in report['rows']:
        print(row['reference']['name'],row['reference']['size'],row['candidate']['size'],'changed aligned bytes',row['different_aligned_bytes'],'relocs equal',row['relocations_equal'])

if __name__=='__main__':main()
