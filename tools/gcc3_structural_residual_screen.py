#!/usr/bin/env python3
"""Nominate small C residuals by mnemonic differences, without claiming causality."""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d
from gcc3_alignment_stack_screen import inventory


def counts(instructions):
    return Counter(m for m,o in instructions if not d.b._padding(m,o))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects',type=Path,required=True)
    parser.add_argument('--census',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    original=inventory(Path(d.b.BLOB))
    exact=set(json.loads(args.census.read_text())['exact_symbols'])
    rows=[];objects=shared=0
    for line in (args.objects/'tc_manifest.txt').read_text().splitlines():
        name,src=line.split()
        if not src.endswith('.c') or src.startswith('src/fax/'):
            continue
        objects+=1;path=args.objects/name
        for symbol,current in inventory(path).items():
            if symbol not in original:
                continue
            shared+=1;left=original[symbol]
            if symbol in exact or left['size']>350:
                continue
            a,b=counts(left['original']),counts(current['original'])
            rows.append({'name':symbol,'source':src,'object':name,
                         'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                         'sizes':[left['size'],current['size']],
                         'mnemonic_distance':sum((a-b).values())+sum((b-a).values()),
                         'original_extra':sorted((a-b).items()),'current_extra':sorted((b-a).items())})
    assert rows and objects and shared
    # A known omitted-call positive and an exact-function negative.
    path=args.objects/'src_pump_v23_v23.c.o'
    old=inventory(path)
    assert counts(original['v23_create']['original'])['call']-counts(old['v23_create']['original'])['call']==1
    assert counts(original['dp_v23_init']['original'])==counts(old['dp_v23_init']['original'])
    rows.sort(key=lambda r:(r['mnemonic_distance'],abs(r['sizes'][0]-r['sizes'][1]),r['sizes'][0],r['name']))
    report={'objects':objects,'shared_C_bodies':shared,'eligible_residual_copies':len(rows),
            'config':(args.objects/'.build-config').read_text(),
            'census_sha256':hashlib.sha256(args.census.read_bytes()).hexdigest(),
            'missing_create_call_positive':True,'exact_init_negative':True,
            'limits':'C only, original <=350B, excludes fax paths. Mnemonics omit recognized padding, retain physical widths. Counts do not prove dataflow, source recovery, reachability or optimizer causality.',
            'rows':rows}
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(objects,'C data/voice TUs;',shared,'shared bodies;',len(rows),'residual copies <=350B; positive/negative controls pass')


if __name__=='__main__':
    main()
