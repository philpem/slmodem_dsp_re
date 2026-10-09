#!/usr/bin/env python3
"""Read-only binary witnesses for call-relative memory timing and shared tails.

Displacements are clues, not field identities: different base registers may
refer to unrelated objects. Epoch deltas do not prove aliasing or source age.
Only equal call-count bodies enter the layout comparison. These epochs count
preceding calls in assembly order, not calls on a reachable path. No source scoring.
"""
import argparse, hashlib, json, re
from collections import Counter
from pathlib import Path
from gcc_x87_transfer_screen import assembly
import playbook_small_patterns as d


def operands(text):
    result=[]; start=depth=0
    for i,c in enumerate(text):
        depth += (c=='(')-(c==')')
        if c==',' and depth==0:
            result.append(text[start:i]); start=i+1
    return result+[text[start:]]


def bounded(rows,size):
    assert rows and size > 0
    start=rows[0][0]
    return [r for r in rows if start <= r[0] < start+size]


def features(rows):
    epoch=0; accesses=[]; edges=[]; by_address={r[0]:r for r in rows}
    for address,mnemonic,operand in rows:
        args=operands(operand)
        if mnemonic.startswith('j') and not operand.startswith('*'):
            dest=re.match(r'([0-9a-f]+)\s',operand)
            target=by_address.get(int(dest[1],16)) if dest else None
            if target and target[1].startswith('mov') and re.search(r'\(%(?!esp|ebp)',target[2]):
                edges.append({'branch':address,'target':target[0],'target_instruction':target[1:]})
        if mnemonic=='call': epoch+=1; continue
        if mnemonic.startswith(('lea','nop')): continue
        for i,arg in enumerate(args):
            match=re.fullmatch(r'(0x[0-9a-f]+|-0x[0-9a-f]+)?\(%(eax|ebx|ecx|edx|esi|edi)\)',arg)
            if not match:continue
            kind='write' if i==len(args)-1 and mnemonic.startswith(('mov','fst','fist')) else 'read'
            if i==len(args)-1 and mnemonic.startswith(('add','sub','and','or','xor','inc','dec')):kind='read-write'
            accesses.append({'address':address,'epoch':epoch,'kind':kind,'offset':int(match[1] or '0',16),
                             'base':match[2],'instruction':[mnemonic,operand]})
    return {'calls':epoch,'returns':sum(r[1]=='ret' for r in rows),'memory_accesses':accesses,'reload_target_edges':edges}


def compare(original,retained):
    a=features(original);b=features(retained);delta=[]
    if a['calls']==b['calls'] and a['calls']:
        def counts(f):
            return Counter((r['offset'],r['kind'],r['epoch']) for r in f['memory_accesses'])
        x,y=counts(a),counts(b)
        offsets={k[:2] for k in x}|{k[:2] for k in y}
        for offset,kind in sorted(offsets):
            left=[x[(offset,kind,e)] for e in range(a['calls']+1)]
            right=[y[(offset,kind,e)] for e in range(a['calls']+1)]
            if left!=right:delta.append({'offset':offset,'kind':kind,'original_epochs':left,'retained_epochs':right})
    return {'original':a,'retained':b,'call_epoch_deltas':delta,
            'extra_retained_returns':b['returns']-a['returns'],
            'reload_edge_count_delta':len(b['reload_target_edges'])-len(a['reload_target_edges'])}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects',type=Path,required=True)
    ap.add_argument('--census',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--max-bytes',type=int,default=850)
    ap.add_argument('--known-old-objects',type=Path,required=True)
    args=ap.parse_args();root=d.ROOT
    blob=assembly(d.b.BLOB);sizes=d.b.sizes(d.b.BLOB)
    blob={n:bounded(rows,sizes[n]) for n,rows in blob.items() if n in sizes}
    exact=set(json.loads(args.census.read_text())['exact_symbols'])
    counts=Counter();candidates=[];controls=[]
    for symbol,path in [('V17TX_control','src/fax/V17t_stc.c'),('fax_class1_status','src/fax/class1.c')]:
        old=assembly(args.known_old_objects/(path.replace('/','_')+'.o'))
        current=assembly(args.objects/(path.replace('/','_')+'.o'))
        oldsize=d.b.sizes(str(args.known_old_objects/(path.replace('/','_')+'.o')))[symbol]
        newsize=d.b.sizes(str(args.objects/(path.replace('/','_')+'.o')))[symbol]
        before=compare(blob[symbol],bounded(old[symbol],oldsize));after=compare(blob[symbol],bounded(current[symbol],newsize))
        assert before['call_epoch_deltas'] or before['extra_retained_returns'] or before['reload_edge_count_delta'],symbol
        assert not after['call_epoch_deltas'] and not after['extra_retained_returns'] and not after['reload_edge_count_delta'],symbol
        controls.append({'symbol':symbol,'historical':before,'retained':after})
    for source in sorted((root/'src').rglob('*')):
        if source.suffix not in ('.c','.cpp'):continue
        rel=source.relative_to(root)
        if 'v34' in rel.parts or 'fax' in rel.parts or source.name.startswith(('V34','vpcm')):continue
        counts['scoped_source_TUs']+=1
        obj=args.objects/(str(rel).replace('/','_')+'.o')
        assert obj.exists(),str(obj)
        ours=assembly(obj);oursizes=d.b.sizes(str(obj))
        for symbol,rows in ours.items():
            if symbol not in sizes or symbol not in oursizes:continue
            counts['shared_emitted_bodies']+=1
            if symbol in exact:counts['already_exact']+=1;continue
            if not 0<sizes[symbol]<=args.max_bytes:continue
            counts['small_nonexact_bodies']+=1
            if sizes[symbol]<=16:
                counts['tiny_bodies_excluded']+=1;continue
            evidence=compare(blob[symbol],bounded(rows,oursizes[symbol]))
            if evidence['original']['calls']==evidence['retained']['calls'] and evidence['original']['calls']:
                counts['equal_nonzero_call_counts']+=1
            if not (evidence['call_epoch_deltas'] or evidence['extra_retained_returns']>0 or evidence['reload_edge_count_delta']):continue
            counts['nominated']+=1
            candidates.append({'source':str(rel),'symbol':symbol,'original_bytes':sizes[symbol],**evidence})
    candidates.sort(key=lambda r:(-int(r['extra_retained_returns']>0),-int(bool(r['reload_edge_count_delta'])),-len(r['call_epoch_deltas']),r['original_bytes']))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({'counts':dict(counts),'census_sha256':hashlib.sha256(args.census.read_bytes()).hexdigest(),
        'build_config':(args.objects/'.build-config').read_text(),'object_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(args.objects.glob('*.o'))},'known_controls':controls,'candidates':candidates,
        'limits':['Different bases can represent different objects; no field identity inferred.',
                  'Equal call count does not establish equal callee identities; review relocation targets.',
                  'Call epochs are linear layout positions, not execution order; diagnostics are included. No semantic aliasing is proved.',
                  'No x87 memory-stack or indexed accesses; static screen is incomplete.',
                  'V34/shared structures and fax excluded; fax has separate ownership.']},indent=2)+'\n')
    print(dict(counts));print('2 historical positive / 2 current exact negative controls passed')
    for r in candidates[:35]:print(r['symbol'],r['source'],r['original_bytes'],'epoch-deltas',len(r['call_epoch_deltas']),'extra-returns',r['extra_retained_returns'],'reload-edges',r['reload_edge_count_delta'])

if __name__=='__main__':main()
