#!/usr/bin/env python3
"""Read-only per-function diagnostic vectors; never an acceptance score.

Each supplied candidate is reported independently. One heuristic instruction
map conditions every axis. Padding remains in that map. Unknown typed effects,
indirect successors and unresolved relocation identities are not matches.
Whole-object/link-selected comparisons belong to partialcmp, not this report.
"""
import argparse
import collections
import difflib
import hashlib
import io
import json
import platform
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import byteident as b
from callee_effects import decode
from jumptable import Refused, instructions, require
from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError
from elftools.elf.relocation import RelocationSection

VERSION = 1
MAPPING = 'mnemonic-anchors-ordinal-replacement-v1'
REG = re.compile(r'%[a-z][a-z0-9]*(?:\([0-9]+\))?')
JCC = frozenset(('ja','jae','jb','jbe','je','jne','jg','jge','jl','jle',
                 'jo','jno','js','jns','jp','jnp'))
# Normal-completion CFG only: exceptions, interrupts and asynchronous entries
# are outside scope. Unlisted undecoded operations have unknown successors.
FALLTHROUGH = frozenset(('lea','inc','dec','xor','adc','sbb','cmp','imul','mul',
    'div','idiv','neg','not','xchg','sahf','lahf','cld','cwtd','cltd','cwtl',
    'mov','movl','movw','movb','leave','push','pop','pushf','popf',
    'fld','flds','fldl','fldt','fld1','fldz','fst','fstp','fsts','fstps','fstl',
    'fstpl','fstpt','filds','fildl','fildll','fists','fistl','fistps','fistpl',
    'fistpll','fadd','faddp','fadds','faddl','fsub','fsubp','fsubr','fsubrp',
    'fsubs','fsubl','fmul','fmulp','fmuls','fmull','fdiv','fdivp','fdivr',
    'fdivrp','fdivs','fdivl','fxch','fsqrt','fabs','fchs','fcom','fcomp',
    'fcompp','fcoms','fcomps','fcoml','fcompl','fucom','fucomp','fucompp',
    'fnstsw','fnstcw','fldcw','fstcw','fstsw'))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def jsonable(value):
    if isinstance(value, bytes):
        return {'bytes_hex': value.hex()}
    if isinstance(value, dict):
        return {str(k): jsonable(v) for k,v in value.items()}
    if isinstance(value, (tuple, list)):
        return [jsonable(v) for v in value]
    return value


def record(row, value, unknown=None, key=None):
    return {'key': row['index'] if key is None else key,
            'instruction_index': row.get('index'), 'offset': row.get('offset'),
            'mnemonic': row.get('mnemonic'), 'operands': row.get('operands'),
            'value': jsonable(value), 'unknown_reason': unknown}


def axis(left, right, mapping, unit):
    """Paired unknowns are disjoint from matched/mismatched and unpaired rows.

    reference_count = matched + mismatched + unknown + reference_only;
    candidate_count = matched + mismatched + unknown + candidate_only.
    Unpaired records keep their own unknown_reason in the full delta list.
    """
    groups = collections.defaultdict(list)
    for j,r in enumerate(right):
        groups[r['key']].append(j)
    used = set()
    result = dict(unit=unit, mapping=MAPPING, reference_count=len(left),
                  candidate_count=len(right), matched=0, mismatched=0,
                  reference_only=0, candidate_only=0, unknown=0, deltas=[])
    for a in left:
        key = mapping.get(a['key']) if isinstance(a['key'], int) else a['key']
        possible = [j for j in groups.get(key, []) if j not in used]
        if not possible:
            status, c = 'reference_only', None
        else:
            j = possible[0]; used.add(j); c = right[j]
            if a['unknown_reason'] or c['unknown_reason']:
                status = 'unknown'
            else:
                status = 'matched' if a.get('comparison', a['value']) == c.get('comparison', c['value']) else 'mismatched'
        result[status] += 1
        if status != 'matched':
            result['deltas'].append({'status': status, 'reference': a, 'candidate': c})
    for j,c in enumerate(right):
        if j not in used:
            result['candidate_only'] += 1
            result['deltas'].append({'status': 'candidate_only', 'reference': None, 'candidate': c})
    result['first_delta'] = next(iter(result['deltas']), None)
    result['reference_unknown_records'] = sum(r['unknown_reason'] is not None for r in left)
    result['candidate_unknown_records'] = sum(r['unknown_reason'] is not None for r in right)
    result['status'] = 'partial' if result['unknown'] or any(
        r['unknown_reason'] for r in left+right) else 'observed'
    if not left and not right:result['status']='not_applicable'
    return result


def correspondence(left, right):
    a = [r['mnemonic'] for r in left]; c = [r['mnemonic'] for r in right]
    pairs = []
    for tag, i, end, j, stop in difflib.SequenceMatcher(a=a,b=c,autojunk=False).get_opcodes():
        if tag in ('equal', 'replace'):
            pairs.extend(zip(range(i,end),range(j,stop)))
    counts = collections.Counter(a) | collections.Counter(c)
    repeats = sorted(k for k,v in counts.items() if v > 1)
    return dict(pairs), {'id': MAPPING, 'pairs': [list(p) for p in pairs],
        'policy': 'exact mnemonic anchors; ordinal pairing within replacement gaps; all padding retained',
        'kind': 'heuristic, not a CFG/dataflow certificate',
        'uniqueness': 'not established', 'ambiguous': bool(repeats),
        'repeated_anchor_keys': repeats}


def load(path, symbol):
    path = str(Path(path).resolve())
    out = {'path':path, 'symbol':symbol, 'status':'unavailable', 'sha256':None}
    body = canonical = None
    try:
        data = Path(path).read_bytes(); out['sha256'] = digest(data)
        elf = ELFFile(io.BytesIO(data))
        require(elf.elfclass == 32 and elf.little_endian and elf['e_machine'] == 'EM_386'
                and elf['e_type'] == 'ET_REL', 'only ELF32 little-endian i386 relocatable objects supported')
        comment = elf.get_section_by_name('.comment')
        out['compiler_comments'] = comment.data().decode('utf-8','replace').strip('\0').split('\0') if comment else []
        out['compiler_flags'] = {'status':'unknown', 'reason':'not recoverable from supplied ELF; no build flags inferred'}
        owner = b.owned_function(path,symbol)
        require(owner is not None, 'missing, ambiguous or unsupported owned function')
        section, base, raw, _ = owner
        out.update(section=section, start=base, declared_bytes=len(raw))
        body, canonical = b.body(path,symbol)
        require(body is not None, 'byteident rejected owned body or relocations')
        seq = b.insns(path,symbol)
        rows = instructions(path,section,base,base+len(raw))
        require(tuple(at-base for at,_,_,_ in rows) == seq.offsets,
                'owned decoder and byteident instruction boundaries disagree')
        require(b''.join(code for _,code,_,_ in rows) == raw, 'decoded bytes differ from owned bytes')
        relocs=[]
        sec_index = elf.get_section_index(section)
        for rs in elf.iter_sections():
            if not isinstance(rs,RelocationSection) or rs['sh_info'] != sec_index:
                continue
            table=elf.get_section(rs['sh_link'])
            for rel in rs.iter_relocations():
                off=rel['r_offset']-base
                if not 0 <= off < len(raw): continue
                target=table.get_symbol(rel['r_info_sym'])
                kind,identity=canonical[off]
                ri=next(i for i,(at,code,_,_) in enumerate(rows) if at-base <= off < at-base+len(code))
                relocs.append({'offset':off,'instruction_index':ri,'type':kind,
                    'raw_target':target.name,'raw_target_section':target['st_shndx'],
                    'raw_addend_signed':int.from_bytes(raw[off:off+4],'little',signed=True),
                    'canonical_target':jsonable(identity),
                    'unknown_reason':'unresolved section destination' if identity[0]=='section' else None})
        decoded=[]
        for i,((at,code,mn,ops), canonical_row) in enumerate(zip(rows,seq)):
            ops=re.sub(r'<[^>]*>','',ops).split('#',1)[0].strip()
            row={'index':i,'offset':at-base,'size':len(code),'raw_hex':code.hex(),
                 'mnemonic':mn,'operands':ops,'canonical_instruction':list(canonical_row),
                 'padding':b._padding(mn,ops),'relocations':[r for r in relocs if r['instruction_index']==i]}
            try:
                kind,args,target=decode(at,code)
                row['typed']={'kind':kind,'operands':jsonable(args),'branch_target':target-base if target is not None else None}
                row['typed_unknown']=None
            except Refused as error:
                row['typed']=None; row['typed_unknown']=str(error)
            decoded.append(row)
        out.update(status='available',decoded_bytes=sum(r['size'] for r in decoded),
                   ordinary_instructions=sum(not r['padding'] for r in decoded),
                   padding_rows=sum(r['padding'] for r in decoded),
                   padding_bytes=sum(r['size'] for r in decoded if r['padding']),
                   instructions=decoded,relocations=relocs)
        return out,body,canonical,seq
    except (OSError,ValueError,KeyError,IndexError,ELFError,subprocess.SubprocessError) as error:
        out['reason']=str(error)
        return out,body,canonical,None


def cfg(obj):
    rows=obj['instructions']; byoff={r['offset']:r['index'] for r in rows}
    edges=[]; leaders={0}; unknown=[]; terminals=set()
    for r in rows:
        i=r['index']; mn=r['mnemonic']; typed=r['typed']; following=i+1 if i+1<len(rows) else None
        control=mn in JCC or mn in ('jmp','call','ret','retl')
        supported=control or typed is not None or mn in FALLTHROUGH or mn.startswith(('nop','set','cmov'))
        def edge(kind,target,why=None):
            edges.append(record(r,{'kind':kind,'condition':mn if mn in JCC else None,'target':target},why))
        if control:
            terminals.add(i)
            if following is not None: leaders.add(following)
            if mn.startswith('ret'):
                edge('return',{'kind':'exit'})
            else:
                rels=r['relocations']
                raw=bytes.fromhex(r['raw_hex'])
                direct=(mn=='call' and len(raw)==5 and raw[0]==0xe8 or
                        mn=='jmp' and ((len(raw)==5 and raw[0]==0xe9) or
                                       (len(raw)==2 and raw[0]==0xeb)) or
                        mn in JCC and ((len(raw)==2 and 0x70<=raw[0]<=0x7f) or
                                       (len(raw)==6 and raw[0]==0x0f and 0x80<=raw[1]<=0x8f)))
                if direct and len(rels)==1 and rels[0]['type']=='R_386_PC32':
                    target={'kind':'external','identity':rels[0]['canonical_target']}
                    why=rels[0]['unknown_reason']
                elif direct and not rels and re.fullmatch(r'[0-9a-f]+',r['operands']):
                    offset=int(r['operands'],16)-obj['start']
                    if offset in byoff:
                        target={'kind':'local','index':byoff[offset],'offset':offset}; why=None
                        leaders.add(byoff[offset])
                    else:
                        target={'kind':'unresolved','offset':offset}; why='target outside owned body or not an instruction boundary'
                else:
                    target={'kind':'unresolved','operand':r['operands']};why='indirect or unsupported control target'
                edge('call' if mn=='call' else 'taken',target,why)
                if mn in JCC or mn=='call':
                    edge('fallthrough',{'kind':'local','index':following,
                         'offset':rows[following]['offset'] if following is not None else None},
                         None if following is not None else 'fallthrough leaves owned function')
        elif not supported:
            terminals.add(i); unknown.append(i)
            if following is not None:leaders.add(following)
            edge('unknown',{'kind':'unresolved'},'unsupported normal-completion control effect')
    starts=sorted(leaders);blocks=[]
    for n,start in enumerate(starts):
        end=starts[n+1]-1 if n+1<len(starts) else len(rows)-1
        blocks.append(record(rows[start],{'start_index':start,'end_index':end}))
        if end not in terminals:
            following=end+1 if end+1<len(rows) else None
            edges.append(record(rows[end],{'kind':'fallthrough','condition':None,
                'target':{'kind':'local','index':following,'offset':rows[following]['offset'] if following is not None else None}},
                None if following is not None else 'fallthrough leaves owned function'))
    return {'blocks':blocks,'edges':edges,'unsupported_control_rows':unknown,
            'convention':'normal-completion graph, calls split blocks; no exception/interrupt or external-entry proof',
            'complete':not any(r['unknown_reason'] for r in edges)}


def row_axes(obj):
    axes={k:[] for k in ('encoding','mnemonics','typed_operations','immediates','addressing',
                        'gp_registers','x87_registers','stack_observations','branch_displacements',
                        'relocation_destinations','relocation_sites')}
    for r in obj['instructions']:
        axes['encoding'].append(record(r,r['raw_hex']))
        axes['mnemonics'].append(record(r,r['mnemonic']))
        typed=r['typed']; why=r['typed_unknown']
        args=typed['operands'] if typed else None
        # Register identity/address values belong to their own literal axes.
        shape=[(a[0],a[2],a[3]) if a[0]=='r' else (a[0],a[5]) if a[0]=='m' else (a[0],a[2]) for a in args] if args is not None else None
        axes['typed_operations'].append(record(r,{'kind':typed['kind'],'operand_widths':shape} if typed else None,why))
        immediates=[a for a in args if a[0]=='i'] if args is not None else None
        if immediates or why:
            axes['immediates'].append(record(r,immediates,
                'relocated immediate ownership is not decoded' if immediates and r['relocations'] else why))
        memory=[] if args is not None else None
        # A relocation-owned displacement is not a literal address identity.
        for a in args or []:
            if a[0]=='m':
                relocated=a[6] is not None and any(rel['offset']==r['offset']+a[6] for rel in r['relocations'])
                memory.append(a[:4]+['<relocation field>',a[5]] if relocated else a[:6])
        if memory or why:axes['addressing'].append(record(r,memory,why))
        tokens=REG.findall(r['operands'])
        gp=[x for x in tokens if x[1:] in b.REG32]
        x87=[x for x in tokens if x.startswith('%st')]
        if gp:axes['gp_registers'].append(record(r,gp))
        if x87:axes['x87_registers'].append(record(r,x87))
        if any(t in ('%esp','%ebp','%sp','%bp') for t in tokens) or r['mnemonic'] in ('push','pop','leave','ret','retl'):
            axes['stack_observations'].append(record(r,[r['mnemonic'],r['operands']]))
        if r['mnemonic'] in JCC or r['mnemonic'] in ('jmp','call'):
            delta=(typed['branch_target']-r['offset']-r['size']) if typed and typed['branch_target'] is not None and not r['relocations'] else None
            axes['branch_displacements'].append(record(r,delta,None if delta is not None else 'encoded displacement not decoded or relocated'))
        for rel in r['relocations']:
            axes['relocation_destinations'].append(record(r,[rel['type'],rel['canonical_target']],rel['unknown_reason']))
            axes['relocation_sites'].append(record(r,[rel['offset'],rel['offset']-r['offset']]))
    return axes


def compare(reference,candidate):
    ro,rb,rr,rs=reference;co,cb,cr,cs=candidate
    report={'reference':ro,'candidate':co,'scope':'one explicitly supplied object copy; no whole-tree or link-selected inference'}
    if ro['status']!='available' or co['status']!='available':
        diagnostic,amount=b.verdict(rb,rr,cb,cr)
        grade=None if diagnostic in ('SIZE','BYTES') else ('NONMATCH' if diagnostic=='RELOC' else diagnostic)
        report.update(strict_identity={'grade':grade,'diagnostic':diagnostic,'difference_count':amount,
                      'reason':'instruction evidence unavailable; no alpha proof attempted'},
                      axes=None,mapping=None,unknown_evidence=True)
        return report
    diagnostic,amount=b.verdict(rb,rr,cb,cr)
    reason=b.alpha_why(rs,cs) if diagnostic in ('SIZE','BYTES') else None
    grade='REGALLOC' if diagnostic in ('SIZE','BYTES') and reason is None else (
          'NONMATCH' if diagnostic in ('SIZE','BYTES','RELOC') else diagnostic)
    report['strict_identity']={'grade':grade,'diagnostic':diagnostic,'difference_count':amount,
        'alpha_refusal':reason,'authority':'byteident.verdict; alpha_why only for SIZE/BYTES',
        'frame_certificate':jsonable(b.framefree_certificate(rs,cs)) if diagnostic in ('SIZE','BYTES') else None}
    mapping,description=correspondence(ro['instructions'],co['instructions']);report['mapping']=description
    left=row_axes(ro);right=row_axes(co)
    graph_a,graph_b=cfg(ro),cfg(co)
    report['cfg']={'reference':graph_a,'candidate':graph_b}
    for item in graph_a['edges']:
        value=item['value'];target=value['target'];comp=dict(value)
        if target['kind']=='local':
            index=target['index']
            comp['target']={'kind':'local','index':mapping.get(index)}
            if index not in mapping:item['unknown_reason']=item['unknown_reason'] or 'target has no instruction correspondence'
        item['comparison']=comp
    for item in graph_b['edges']:
        value=item['value'];target=value['target'];comp=dict(value)
        if target['kind']=='local':comp['target']={'kind':'local','index':target['index']}
        item['comparison']=comp
    for item in graph_a['blocks']:
        v=item['value']; item['comparison']={k:mapping.get(i) for k,i in v.items()}
        if any(i not in mapping for i in v.values()):item['unknown_reason']='block boundary lacks instruction correspondence'
    left.update(cfg_edges=graph_a['edges'],cfg_blocks=graph_a['blocks'])
    right.update(cfg_edges=graph_b['edges'],cfg_blocks=graph_b['blocks'])
    for name in ('extent','placement','padding'):
        fields={'extent':('declared_bytes','decoded_bytes','ordinary_instructions'),
                'placement':('section','start'),'padding':('padding_rows','padding_bytes')}[name]
        left[name]=[record({'index':None,'offset':0},ro[k],key=k) for k in fields]
        right[name]=[record({'index':None,'offset':0},co[k],key=k) for k in fields]
    report['axes']={k:axis(left[k],right[k],mapping,
        'explicit register-bearing instruction records' if k in ('gp_registers','x87_registers') else
        'memory-bearing instruction records or unknown decoder rows' if k=='addressing' else
        'immediate-bearing instruction records or unknown decoder rows' if k=='immediates' else
        'edge records' if k=='cfg_edges' else 'block records' if k=='cfg_blocks' else
        'relocation records' if k.startswith('relocation') else 'named observations' if k in ('extent','placement','padding') else 'instruction records') for k in left}
    report['stack_and_abi']={'slot_correspondence':None,'return_contract':None,'implicit_effects':'not inferred',
        'literal_observations':'ESP/EBP operands and push/pop/leave/return text only',
        'proof':'only the existing frame_certificate, when present, establishes its own narrow frame assumptions'}
    report['unknown_evidence']=any(a['status']=='partial' for a in report['axes'].values())
    return report


def report(reference,candidates,symbol,reference_symbol=None):
    previous=b.LINK_RESOLUTION
    try:
        return _report(reference,candidates,symbol,reference_symbol)
    finally:
        b.LINK_RESOLUTION=previous
        clear_caches()


def clear_caches():
    import jumptable
    import guarded_switch
    for fn in (b.body,b.function_owners,b.section_symbols,instructions,
               jumptable.section_direct_edges,guarded_switch.section_direct_edges):
        fn.cache_clear()


def _report(reference,candidates,symbol,reference_symbol=None):
    # Never borrow unrelated default partial-link artifacts. Existing local
    # certificates still run through byteident and are reflected in its grade.
    b.LINK_RESOLUTION=(None,None)
    clear_caches()
    ref=load(reference,reference_symbol or symbol)
    copies=[compare(ref,load(path,symbol)) for path in candidates]
    versions={}
    for command in ('objdump','readelf','nm'):
        try:versions[command]=subprocess.check_output([command,'--version'],text=True).splitlines()[0]
        except (OSError,subprocess.SubprocessError) as error:versions[command]=str(error)
    return {'schema_version':VERSION,'tool':'diffvector','provided_candidate_copies':len(copies),
        'scope':'supplied copies only; no global best-copy or aggregate fidelity grade',
        'instruction_mapping':MAPPING,'linked_resolution_artifacts':None,
        'python':platform.python_version(),'tool_versions':versions,
        'out_of_scope':{'referenced_data_contents':'not measured; use partialcmp for whole-object data',
                       'whole_object_identity':'not measured; use partialcmp',
                       'runtime_or_fixture_memory_context':'not inferred from function bytes'},
        'implementation_sha256':{name:digest((HERE/name).read_bytes()) for name in
            ('diffvector.py','byteident.py','framefree.py','callee_effects.py','jumptable.py',
             'guarded_switch.py','induction_jumptable.py','readonly_copy.py')},
        'limitations':['heuristic correspondence; repeated rows may admit other mappings',
            'padding is observed but never discarded from CFG correspondence',
            'typed operand decoder is bounded; x87 literals are observed but typed x87 effects remain unknown',
            'explicit register tokens only; implicit GP/x87 effects and ABI return contract are unknown',
            'whole-object placement/link identity must be measured separately with partialcmp'],
        'copies':copies}


def human(result):
    print('diffvector v%d: %d supplied copies; diagnostic only, no aggregate grade' % (VERSION,result['provided_candidate_copies']))
    for c in result['copies']:
        print('%s: %s (%s)' % (c['candidate']['path'],c['strict_identity']['grade'] or 'unavailable',c['strict_identity']['diagnostic']))
        if c['axes'] is None:
            print('  unavailable: '+str(c['candidate'].get('reason',c['reference'].get('reason'))));continue
        a,z=c['reference'],c['candidate']
        print('  bytes declared=%d/%d decoded=%d/%d; instructions ordinary=%d/%d padding=%d/%d (%d/%d bytes)' %
              (a['declared_bytes'],z['declared_bytes'],a['decoded_bytes'],z['decoded_bytes'],
               a['ordinary_instructions'],z['ordinary_instructions'],a['padding_rows'],z['padding_rows'],a['padding_bytes'],z['padding_bytes']))
        ga,gz=c['cfg']['reference'],c['cfg']['candidate']
        print('  CFG blocks=%d/%d edges=%d/%d; all paired quantities are reference/candidate' %
              (len(ga['blocks']),len(gz['blocks']),len(ga['edges']),len(gz['edges'])))
        print('  map=%s; uniqueness unproved; repeated anchors=%s' % (MAPPING,c['mapping']['ambiguous']))
        for name,a in c['axes'].items():
            label='encoding (raw relocation bytes retained)' if name=='encoding' else name
            print('  %s [%s; %s]: ref=%d cand=%d matched=%d mismatch=%d only=%d/%d unknown-pairs=%d unknown-records=%d/%d' %
                  (label,a['unit'],a['status'],a['reference_count'],a['candidate_count'],a['matched'],a['mismatched'],a['reference_only'],a['candidate_only'],a['unknown'],a['reference_unknown_records'],a['candidate_unknown_records']))
            d=a['first_delta']
            if d:
                def brief(row):
                    if row is None:return 'unpaired'
                    text='%s %s %s => %s' % (row['key'],row['mnemonic'] or '',row['operands'] or '',json.dumps(row['value'],separators=(',',':')))
                    return '@%s %s' % (row['offset'],text[:110]+('...' if len(text)>110 else ''))
                print('    first %s: ref %s | cand %s' % (d['status'],brief(d['reference']),brief(d['candidate'])))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',required=True);p.add_argument('--candidate',action='append',required=True)
    p.add_argument('--symbol',required=True);p.add_argument('--reference-symbol');p.add_argument('--json',type=Path,required=True)
    args=p.parse_args()
    for source in [args.reference]+args.candidate:
        path=Path(source)
        if args.json.resolve()==path.resolve() or (args.json.exists() and path.exists() and args.json.samefile(path)):
            p.error('--json must not alias any reference/candidate input')
    result=report(args.reference,args.candidate,args.symbol,args.reference_symbol)
    args.json.parent.mkdir(parents=True,exist_ok=True)
    args.json.write_text(json.dumps(result,indent=2)+'\n');human(result)
    return 0 if all(c['axes'] is not None for c in result['copies']) else 1


if __name__=='__main__':
    sys.exit(main())
