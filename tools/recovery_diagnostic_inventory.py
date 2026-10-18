#!/usr/bin/env python3
"""Bounded native inventory: explicit SF member store then direct memory compare.

Lexical nominations, not CFG/alias proofs. No compilation or source mutation.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import recovery_float_decision_screen as s
from elftools.elf.elffile import ELFFile
import playbook_small_patterns as d


def member(i):
    if not i.operands or i.operands[0].type != 3:
        return None
    op = i.operands[0]
    base = i.reg_name(op.mem.base)
    if op.size != 4 or not base or base in ('esp', 'ebp') or op.mem.index:
        return None
    return base, op.mem.disp


def row(i):
    return dict(address=hex(i.address), bytes=i.bytes.hex(), mnemonic=i.mnemonic, operands=i.op_str)


def pairs(rows):
    stores = {}
    result = []
    for k, i in enumerate(rows):
        key = member(i)
        if key and i.mnemonic in ('fst', 'fstp'):
            stores[key] = k
        if key and i.mnemonic in ('fcom', 'fcomp') and key in stores:
            j = stores[key]
            middle = rows[j+1:k]
            result.append(dict(store=row(rows[j]), compare=row(i), key=key,
                intervening_calls=[row(x) for x in middle if x.mnemonic == 'call'],
                intervening_branches=[row(x) for x in middle if x.mnemonic.startswith('j')],
                gap=k-j))
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-dir',type=Path,default=s.ROOT/'build/copy293-baseline')
    ap.add_argument('--out',type=Path,default=s.ROOT/'build/diagnostic-witness293')
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    blob=Path(d.b.BLOB).resolve(); functions=s.load(blob)
    # Relocation identity proves diagnostic targets; no symbol-name inference.
    with blob.open('rb') as stream:
        elf=ELFFile(stream); symtab=elf.get_section_by_name('.symtab')
        textrel=elf.get_section_by_name('.rel.text')
        calls={r['r_offset']-1:symtab.get_symbol(r['r_info_sym']).name
               for r in textrel.iter_relocations() if r['r_info_type']==2}
    objects={}
    baseline_objects={}
    for obj in args.baseline_dir.glob('*.o'):
        baseline_objects[str(obj)]=hashlib.sha256(obj.read_bytes()).hexdigest()
        for name in d.b.sizes(str(obj)):
            objects.setdefault(name,[]).append(obj)
    results=[]
    for name, rows in functions.items():
        for p in pairs(rows):
            addr=int(p['compare']['address'],16)
            following=[i for i in rows if i.address>addr][:20]
            nextcall=next((i for i in following if i.mnemonic=='call'),None)
            p['symbol']=name
            p['following_call']=dict(row(nextcall),target=calls.get(nextcall.address)) if nextcall else None
            p['diagnostic_within_20_instructions']=bool(nextcall and calls.get(nextcall.address) in ('edprintf','dsplibs_debug_printf'))
            p['unique_baseline_object']=str(objects[name][0]) if len(objects.get(name,[]))==1 else None
            p['baseline_verdict']=list(d.b.verdict(*d.b.body(str(blob),name),*d.b.body(str(objects[name][0]),name))) if p['unique_baseline_object'] else None
            results.append(p)
    # Demonstrate firing on the known setters, then remove their stores in
    # the decoded stream. Negative controls test this detector, not the blob.
    controls=[]
    for suffix,offset in [('11setEchoBetaEf',48),('14setDecayFactorEf',52)]:
        name='_ZN16V92EchoCanceller'+suffix;rows=functions[name]
        hit=pairs(rows);assert len(hit)==1 and hit[0]['key'][1]==offset
        address=int(hit[0]['store']['address'],16)
        assert not pairs([i for i in rows if i.address!=address])
        controls.append(dict(symbol=name,known_positive=hit[0]['compare']['address'],removed_store_rejected=True))
    for name in ('_ZN16V92EchoCanceller5resetEv','_ZN12V90PreFilter12setParamEia6Ef'):
        if name in functions:
            assert not pairs(functions[name]);controls.append(dict(symbol=name,negative_no_pair=True))
    summary=dict(functions=len(functions),instructions=sum(map(len,functions.values())),
        explicit_sf_member_stores=sum(i.mnemonic in ('fst','fstp') and member(i) is not None for rs in functions.values() for i in rs),
        direct_sf_member_compares=sum(i.mnemonic in ('fcom','fcomp') and member(i) is not None for rs in functions.values() for i in rs),
        lexical_pairs=len(results),pair_symbols=len({r['symbol'] for r in results}),
        near_diagnostic_pairs=sum(r['diagnostic_within_20_instructions'] for r in results),
        no_intervening_call_pairs=sum(not r['intervening_calls'] for r in results))
    report=dict(blob=str(blob),blob_sha256=hashlib.sha256(blob.read_bytes()).hexdigest(),
        baseline_directory=str(args.baseline_dir.resolve()),
        baseline_config=(args.baseline_dir/'.build-config').read_text(),
        baseline_manifest_sha256=hashlib.sha256((args.baseline_dir/'manifest.json').read_bytes()).hexdigest(),
        baseline_object_sha256=baseline_objects,
        revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=s.ROOT,text=True).strip(),
        summary=summary,controls=controls,results=results,
        limits='Explicit non-indexed SF x87 stores and direct SF memory fcom/fcomp only; ESP and EBP are excluded even when EBP is not a frame pointer. Integer stores, base-register copies, flds then register compare, indexed members, CFG reachability and alias/clobber proofs are outside domain. Lexical address keys are not killed by base-register writes or aliasing stores: pairs are nominations, not identity-of-object proofs. Calls and branches are reported, not assumed safe. A nearby diagnostic call does not prove its data dependency. Baseline verdicts measure bytes and relocations, not source truth. No compiler run or differential execution.')
    (args.out/'inventory.json').write_text(json.dumps(report,indent=2)+'\n')
    witnesses={r['symbol'] for r in results if r['diagnostic_within_20_instructions']}
    for name in witnesses:
        rows=functions[name]
        (args.out/(name+'.dis.txt')).write_text(subprocess.check_output(['python3',str(s.ROOT/'tools/dis.py'),str(blob),hex(rows[0].address),hex(rows[-1].address+rows[-1].size)],text=True,stderr=subprocess.DEVNULL))
    print(json.dumps(summary,sort_keys=True));print('Known-positive/remove-store controls:',len(controls))
    for r in results:
        print(r['symbol'],r['store']['address'],'->',r['compare']['address'],'calls',len(r['intervening_calls']),'diagnostic',r['diagnostic_within_20_instructions'],'baseline',r['baseline_verdict'])

if __name__=='__main__':main()
