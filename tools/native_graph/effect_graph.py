#!/usr/bin/env python3
"""Bounded native/current i386 effect-graph nomination; never equivalence proof.

Uses a frozen owner tracer. Direct CFG only; loops retain static reachability,
while cyclic event relations are excluded. No instruction-order comparison of
disjoint stores. Ambiguous occurrences are never paired by ordinal. Calls have
no semantic summaries. Nested owners require the tracer's first entry epoch.
"""
import argparse, collections, hashlib, json, pathlib, re, sys, tempfile, time, os
HERE=pathlib.Path(__file__).resolve().parent
if __package__:
    from .bootstrap import CONTEXT
else:
    from bootstrap import CONTEXT
ot=CONTEXT.owner
bi,jt=ot.byteident,ot.jumptable
EPOCHS=len(ot.initial())==3
def readvalue(operand,regs,stack,at,epoch=False):
    return ot.value(operand,regs,stack,at,epoch) if EPOCHS else ot.value(operand,regs,stack,at)

def digest(data): return hashlib.sha256(data).hexdigest()
def need(ok,msg):
    if not ok: raise ValueError(msg)
def key(v): return json.dumps(v,sort_keys=True,separators=(',',':'))

def validate_object(path):
    from elftools.elf.elffile import ELFFile
    with pathlib.Path(path).open('rb') as stream:
        elf=ELFFile(stream)
        need(elf.elfclass==32 and elf.little_endian and elf['e_machine']=='EM_386' and elf['e_type']=='ET_REL','requires little-endian ELF32 EM_386 ET_REL')

def extract(path,symbol,max_steps=30000):
    validate_object(path)
    owned=bi.owned_function(str(path),symbol)
    if owned is None: return {'status':'refused','reason':'nonunique/overlapping/unsupported function ownership'}
    sec,start,data,relocs=owned
    try: rows=jt.instructions(str(path),sec,start,start+len(data))
    except ValueError as e: return {'status':'refused','reason':str(e)}
    by={r[0]:r for r in rows}; succ={}; bad=[]
    body,relocations=bi.body(str(path),symbol)
    need(body==data,'decoder body mismatch')
    calls={}
    for at,raw,mn,text in rows:
        if mn=='call':
            rr=[v for off,v in relocations.items() if at-start<=off<at-start+len(raw)]
            if len(rr)==1: calls[at]=('call',rr[0])
            elif not text.startswith('*'):
                match=re.search(r'<([^>]+)>',text)
                if match and '+' not in match[1]: calls[at]=('call',('local',match[1]))
        nxt=at+len(raw); edges=[] if mn.startswith('ret') else ([nxt] if nxt in by else [])
        if mn.startswith('j') or mn.startswith('loop'):
            m=re.match(r'^([0-9a-f]+)(?:\s|$)',text); dest=int(m[1],16) if m else None
            if dest not in by or any(at<=start+o<nxt for o in relocs): bad.append(at)
            else: edges=[dest] if mn=='jmp' else edges+[dest]
        if not mn.startswith('ret') and mn!='jmp' and nxt not in by: bad.append(at)
        succ[at]=edges
    if bad: return {'status':'refused','reason':'indirect/external/relocated CFG or out-of-function fallthrough','instruction_count':len(rows),'unsupported_cfg_sites':bad}
    semantic={}
    for r in rows:
        at,raw,mn,text=r
        semantic[at]=(at,raw,'unsupported-relocation',text) if mn!='call' and any(at<=start+o<at+len(raw) for o in relocs) else r
    incoming={start:ot.initial()}; q=collections.deque([start]); steps=0
    while q:
        at=q.popleft(); steps+=1
        if steps>max_steps: return {'status':'refused','reason':'dataflow convergence bound','instruction_count':len(rows)}
        after,_,_=ot.transfer(semantic[at],incoming[at])
        for dst in succ[at]:
            edge=(after[0],after[1],False) if EPOCHS and dst<=at else after
            merged=ot.merge(incoming[dst],edge) if dst in incoming else edge
            if incoming.get(dst)!=merged: incoming[dst]=merged; q.append(dst)
    def expr(v,bits=32):
        if v[0]=='constant': return ('constant',v[1])
        if v[0]=='address':
            origin=v[1]
            if origin[0]=='entry-arg': return ('argument-value',origin[1],v[2])
            if origin[0] in ('entry-load','pointer-load'):
                owner=origin[2]
                if ot.classify(owner)=='supported' and v[2]==0: return ('captured-read',bits,owner)
            if origin[0]=='call-return' and v[2]==0 and origin[1] in calls and sum(v==calls[origin[1]] for v in calls.values())==1: return ('call-result',calls[origin[1]])
        if v[0]=='extended':
            width=8 if v[1].endswith('bl') else 16
            inner=expr(v[2],width)
            if inner is not None: return ('extend',v[1],inner)
        return None
    predecessors=collections.defaultdict(list)
    for source,targets in succ.items():
        for target in targets:predecessors[target].append(source)
    events=[]; unknown=collections.Counter(); unsupported=[]; mov_stores=0; mov_reads=0
    for at in sorted(incoming):
        r=semantic[at]; _,raw,mn,text=r; regs,stack=incoming[at][:2]; epoch=incoming[at][2] if EPOCHS else False; ops=ot.parts(text)
        _,stores,un=ot.transfer(r,incoming[at])
        if un: unsupported.append({'at':at,'instruction':mn+' '+text})
        for st in stores:
            owner=st['owner']; status=ot.classify(owner)
            if owner[0]=='stack': continue
            mov_stores+=1
            val=expr(st['value'],st['width'])
            if status=='supported' and val is not None:
                events.append({'at':at,'kind':'write','anchor':['write',owner,st['width'],val],'owner':owner,'width':st['width'],'value':val})
            else: unknown['write_'+(status if status!='supported' else 'value')]+=1
        if mn in ('mov','movb','movw','movl','movsbl','movswl','movzbl','movzwl') and len(ops)==2 and not ops[0].startswith(('%','$')):
            owner=ot.address(ops[0],regs)
            if owner[0]!='stack':
                mov_reads+=1; bits=8 if mn in ('movsbl','movzbl') else 16 if mn in ('movswl','movzwl') else ot.width_bits(mn,ops)
                if ot.classify(owner)=='supported' and bits:
                    events.append({'at':at,'kind':'read','anchor':['read',owner,bits],'owner':owner,'width':bits})
                else: unknown['read_'+ot.classify(owner)]+=1
        if mn=='call':
            if at in calls: events.append({'at':at,'kind':'call','anchor':calls[at]})
            else: unknown['indirect_or_unresolved_call']+=1
        if mn.startswith('j') and mn!='jmp':
            # Only flags from a linear cmp/test, with flag-preserving MOV/LEA/NOP
            # between it and the branch. No predecessor-search or guessed phi.
            idx=next(i for i,row in enumerate(rows) if row[0]==at); producer=None
            child=at
            for prev in reversed(rows[:idx]):
                pa,pr,pm,pt=prev
                if predecessors[child]!=[pa]:break
                child=pa
                if pa not in incoming or succ[pa]!=[pa+len(pr)]: break
                if pm in ('cmp','cmpb','cmpw','cmpl','test','testb','testw','testl'): producer=prev; break
                if pm not in ('mov','movb','movw','movl','movzbl','movzwl','movsbl','movswl','lea','nop'): break
            if producer:
                pa,_,pm,pt=producer; ps=incoming[pa]; pop=ot.parts(pt)
                width=8 if pm.endswith('b') else 16 if pm.endswith('w') else 32
                ev=[expr(readvalue(p,ps[0],ps[1],pa,ps[2] if EPOCHS else False),width) for p in pop]
                if len(ev)==2 and all(v is not None for v in ev):
                    width=8 if pm.endswith('b') else 16 if pm.endswith('w') else 32
                    anchor=['branch',pm.rstrip('bwl'),width,ev]
                    events.append({'at':at,'kind':'branch','anchor':anchor,'predicate':mn,'arms':{'fallthrough':succ[at][0],'taken':succ[at][-1]}}); continue
            unknown['branch_predicate']+=1
    return {'status':'bounded','instruction_count':len(rows),'reachable_instructions':len(incoming),'events':events,'unknown':dict(unknown),'unsupported_instructions':unsupported,'mov_write_count':mov_stores,'mov_read_count':mov_reads,'successors':succ,'entry':start,'backedges':[[a,b] for a,ss in succ.items() for b in ss if b<=a]}

def compare(a,b):
    if a['status']!='bounded' or b['status']!='bounded': return {'status':'refused','findings':[],'matched_anchors':0}
    groups=[]
    for report in (a,b):
        g=collections.defaultdict(list)
        for e in report['events']: g[key(e['anchor'])].append(e)
        groups.append(g)
    ga,gb=groups; matched={k:(ga[k][0],gb[k][0]) for k in ga.keys()&gb.keys() if len(ga[k])==len(gb[k])==1}
    ambiguous=[k for k in ga.keys()|gb.keys() if len(ga[k])>1 or len(gb[k])>1]
    unpaired=[{'anchor':json.loads(k),'native_count':len(ga[k]),'candidate_count':len(gb[k])} for k in sorted(ga.keys()|gb.keys()) if len(ga[k])!=len(gb[k])]
    findings=[]
    for k,(x,y) in matched.items():
        if x['kind']=='branch' and x['predicate']!=y['predicate']:
            # Inverse branch layout is handled by arm-truth normalization below.
            if pred(x['predicate'])[0]!=pred(y['predicate'])[0]: findings.append({'kind':'predicate','anchor':x['anchor'],'native':x,'candidate':y})
    def reach(report):
        cache={}; succ=report['successors']
        def walk(start):
            if start not in cache:
                seen=set(); q=list(succ.get(start,[]))
                while q:
                    n=q.pop()
                    if n in seen: continue
                    seen.add(n); q.extend(succ.get(n,[]))
                cache[start]=seen
            return cache[start]
        return walk
    ra,rb=reach(a),reach(b); compared=cyclic=0
    ordered=sorted(matched)
    for i,k in enumerate(ordered):
        for l in ordered[i+1:]:
            x,y=matched[k][0],matched[l][0]
            kinds={x['kind'],y['kind']}
            useful=('branch' in kinds or 'call' in kinds) and kinds not in ({'branch'},{'call'})
            if not useful and kinds<= {'read','write'} and 'write' in kinds:
                useful=x.get('owner')==y.get('owner')
            if not useful: continue
            signatures=[]; is_cycle=False
            for report,r,side in ((a,ra,0),(b,rb,1)):
                p,q=matched[k][side],matched[l][side]; pq=q['at'] in r(p['at']); qp=p['at'] in r(q['at'])
                if pq and qp: is_cycle=True
                sig=[pq,qp]
                for first,other in ((p,q),(q,p)):
                    if first['kind']=='branch':
                        family,invert=pred(first['predicate']); arms=first['arms']
                        val=[]
                        for arm in ('fallthrough','taken'):
                            dest=arms[arm]; val.append(dest==other['at'] or other['at'] in r(dest))
                        sig.append((family,tuple(reversed(val)) if invert else tuple(val)))
                signatures.append(sig)
            if is_cycle: cyclic+=1; continue
            compared+=1
            if signatures[0]!=signatures[1]: findings.append({'kind':'happens_before_or_branch_arm','anchors':[json.loads(k),json.loads(l)],'native_sites':[matched[k][0]['at'],matched[l][0]['at']],'candidate_sites':[matched[k][1]['at'],matched[l][1]['at']],'native_relation':signatures[0],'candidate_relation':signatures[1]})
    return {'status':'bounded','matched_anchors':len(matched),'ambiguous_anchor_count':len(ambiguous),'ambiguous_anchors':[json.loads(k) for k in sorted(ambiguous)],'unpaired':unpaired,'relations_compared':compared,'cyclic_relations_excluded':cyclic,'findings':findings,'semantic_proof':False}

def pred(j):
    aliases={'jz':'je','jnz':'jne','jc':'jb','jnae':'jb','jnb':'jae','jnc':'jae','jna':'jbe','jnbe':'ja','jnge':'jl','jnl':'jge','jng':'jle','jnle':'jg'}; j=aliases.get(j,j)
    for x,y in (('je','jne'),('jb','jae'),('jbe','ja'),('jl','jge'),('jle','jg'),('js','jns'),('jo','jno'),('jp','jnp')):
        if j in (x,y): return (x,j==y)
    return (j,False)

def compact(r):
    return {k:v for k,v in r.items() if k!='successors'}

def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--native',required=True,type=pathlib.Path); p.add_argument('--candidate',required=True,type=pathlib.Path); p.add_argument('--symbol',action='append',required=True); p.add_argument('--output',required=True,type=pathlib.Path); args=p.parse_args()
    need(not args.output.exists(),'output already exists: '+str(args.output))
    inputs={}; functions=[]
    with tempfile.TemporaryDirectory(prefix='native-effect-inputs-') as td:
        snapshots=[]
        for i,path in enumerate((args.native,args.candidate)):
            data=path.read_bytes(); frozen=pathlib.Path(td)/('%d.o'%i); frozen.write_bytes(data); frozen.chmod(0o400); validate_object(frozen); snapshots.append(frozen); inputs[str(path)]={'sha256':digest(data),'size':len(data)}
        for name in args.symbol:
            a,b=(extract(path,name) for path in snapshots); functions.append({'symbol':name,'native':compact(a),'candidate':compact(b),'comparison':compare(a,b)})
    report={'schema':'native-effect-graph-prototype-v1','semantic_proof':False,'inputs':inputs,'functions':functions,'selected':len(functions),'tool_sha256':digest(pathlib.Path(__file__).read_bytes()),'helper_inputs':CONTEXT.manifest(),'bootstrap_sha256':digest((HERE/'bootstrap.py').read_bytes())}
    args.output.write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps({'selected':len(functions),'findings':sum(len(f['comparison']['findings']) for f in functions),'refused':sum(f['comparison']['status']=='refused' for f in functions)}))
if __name__=='__main__': main()
