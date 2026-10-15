#!/usr/bin/env python3
"""Nominate fresh EAX-result extensions after equal named call sequences.

A bounded straight-line screen, not a type inference or general def/use proof.
Stop at calls, control transfers and any intervening EAX/AX/AL/AH write; retain
all native windows so each nomination can be checked against full operands.
"""
import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies, sha
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess


def result_sites(rows, window):
    sites=[]
    for i,row in enumerate(rows):
        if row['mnemonic']!='call' or 'target' not in row:
            continue
        following=[];extension=None
        for ins in rows[i+1:i+1+window]:
            mnemonic,operand=ins['mnemonic'],ins['operands']
            following.append(ins)
            if mnemonic=='cwtl' or (mnemonic in ('movsbl','movzbl','movswl','movzwl') and re.fullmatch(r'%(?:al|ax),%\w+',operand)):
                extension=ins;break
            if mnemonic.startswith(('j','ret','call','loop')) or mnemonic in ('popa','pusha','leave','xchg','mul','imul','div','idiv','lods'):
                break
            # AT&T explicit destination, plus x87 status/one-operand writes.
            if re.search(r'%(?:eax|ax|al|ah)$',operand):
                # CMP/TEST consume their destination-shaped final operand.
                if not mnemonic.startswith(('cmp','test','push')):
                    break
        sites.append({'target':row['target'],'call_address':row['address'],
                      'extension':extension,'following':following})
    return sites


def compare(left,right,window):
    a,b=result_sites(left,window),result_sites(right,window)
    if [x['target'] for x in a]!=[x['target'] for x in b]:return None
    return [{'ordinal':i,'target':x['target'],'original':x,'retained':y}
            for i,(x,y) in enumerate(zip(a,b)) if y['extension'] and not x['extension']]


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects',type=Path,default=d.ROOT/'build/tc_out')
    ap.add_argument('--reference',type=Path,default=Path(d.b.BLOB))
    ap.add_argument('--window',type=int,default=6)
    ap.add_argument('--positive-object',type=Path,default=d.ROOT/'build/gcc3-descrambler-output/V90Phase4Demodulator/baseline/candidate.o')
    ap.add_argument('--output',type=Path,default=d.ROOT/'build/call-result-boundary-screen.json')
    args=ap.parse_args();assert args.window>0
    reference=bodies(args.reference);counts=Counter({k:0 for k in ('objects','emitted_bodies','shared_bodies','without_reference','different_named_call_sequence','equal_named_call_sequence','matched_calls','nominated_bodies','nominated_calls')});nominations=[]
    manifests={}
    for path in sorted(args.objects.glob('*.o')):
        counts['objects']+=1;manifests[path.name]=sha(path)
        for name,rows in bodies(path).items():
            counts['emitted_bodies']+=1
            if name not in reference:counts['without_reference']+=1;continue
            counts['shared_bodies']+=1
            hits=compare(reference[name],rows,args.window)
            if hits is None:counts['different_named_call_sequence']+=1;continue
            counts['equal_named_call_sequence']+=1
            counts['matched_calls']+=len(result_sites(rows,args.window))
            if hits:
                nominations.append({'object':path.name,'symbol':name,'calls':hits})
                counts['nominated_bodies']+=1;counts['nominated_calls']+=len(hits)
    name='_ZN20V90Phase4Demodulator14getV90DecisionEs'
    positive=compare(reference[name],bodies(args.positive_object)[name],args.window)
    assert positive and any(x['target']=='_ZN11DescramblerIhiE7processEh' for x in positive)
    assert compare(reference[name],reference[name],args.window)==[]
    # An intervening EAX write must prevent a false return-provenance claim.
    sample=[{'address':'0','mnemonic':'call','operands':'','target':'f'},
            {'address':'1','mnemonic':'mov','operands':'$0x1,%eax'},
            {'address':'2','mnemonic':'cwtl','operands':''}]
    assert result_sites(sample,6)[0]['extension'] is None
    report={'revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=d.ROOT,text=True).strip(),
            'config':(args.objects/'.build-config').read_text(),'reference_sha256':sha(args.reference),
            'objects':manifests,'window':args.window,'counts':dict(counts),'nominations':nominations,
            'positive_control_nominations':positive,
            'controls':{'historical_descrambler_positive':True,'historical_nominated_calls':len(positive),'identity_negative':True,
                        'intervening_EAX_write_negative':True,'positive_object_sha256':sha(args.positive_object)}}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'counts':report['counts'],'controls':report['controls']},indent=2))
    for n in nominations:print(n['object'],n['symbol'],[(x['ordinal'],x['target']) for x in n['calls']])


if __name__=='__main__':main()
