#!/usr/bin/env python3
"""Screen small floating residual copies; frame gaps are nominations, not causes."""
import argparse
import hashlib
import json
from pathlib import Path
from difflib import SequenceMatcher
import playbook_small_patterns as d


def frames(instructions):
    return [o for m,o in instructions[:20] if m == 'sub' and o.endswith(',%esp')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--census',type=Path,default=d.ROOT/'build/psd-final-census.json')
    parser.add_argument('--out',type=Path,default=d.ROOT/'build/expansion-transfer-screen.json')
    parser.add_argument('--objects',type=Path,default=d.ROOT/'build/expansion-transfer-before-objects')
    args = parser.parse_args()
    census = json.loads(args.census.read_text())
    exact = set(census['exact_symbols'])
    blob = d.b.sizes(d.b.BLOB)
    rows, copies, owners = [], 0, {}
    for path in sorted(args.objects.glob('*.o')):
        for name,size in d.b.sizes(str(path)).items():
            if name not in blob or name in exact:
                continue
            copies += 1
            owners.setdefault(name,[]).append(str(path.relative_to(d.ROOT)))
            if blob[name] > 700:
                continue
            original, current = d.b.insns(d.b.BLOB,name), d.b.insns(str(path),name)
            if not any(m.startswith('f') for m,o in current):
                continue
            changes = [(tag,original[a:b],current[c:e]) for tag,a,b,c,e in
                       SequenceMatcher(a=original,b=current,autojunk=False).get_opcodes() if tag != 'equal']
            rows.append({'name':name,'object':str(path.relative_to(d.ROOT)),
                         'blob_size':blob[name],'our_size':size,
                         'verdict':d.b.verdict(*d.b.body(d.b.BLOB,name),*d.b.body(str(path),name)),
                         'blob_frame_subtractions':frames(original), 'our_frame_subtractions':frames(current),
                         'fp_orig':sum(m.startswith('f') for m,o in original),
                         'fp_ours':sum(m.startswith('f') for m,o in current),'changes':changes})
    old = d.ROOT/'build/psd-final-integration/src_dsp_psd.cpp.o'
    current = d.ROOT/'build/tc_out/src_dsp_psd.cpp.o'
    getter = '_ZNK3Psd14getFrequenciesEPff'
    assert d.b.verdict(*d.b.body(d.b.BLOB,getter),*d.b.body(str(old),getter)) == ('SIZE',1)
    assert d.b.verdict(*d.b.body(d.b.BLOB,getter),*d.b.body(str(current),getter))[0] == 'EXACT'
    assert frames(d.b.insns(str(old),getter)) != frames(d.b.insns(str(current),getter))
    assert copies and rows and len(owners) == census['compared'] - census['exact']
    rows.sort(key=lambda r:(len(r['changes']),abs(r['blob_size']-r['our_size']),r['blob_size']))
    report = {'revision':'3986addf','config':(args.objects/'.build-config').read_text(),
              'census_sha256':hashlib.sha256(args.census.read_bytes()).hexdigest(),
              'object_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in args.objects.glob('*.o')},
              'residual_copies':copies, 'unique_residual_names':len(owners),
              'small_floating_copies':len(rows),'small_floating_names':len({r['name'] for r in rows}),
              'positive_negative_getter_frame_controls':True,'rows':rows}
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print('%d residual copies/%d names; %d small floating copies/%d names; PSD positive/negative control passes' %
          (copies,len(owners),len(rows),report['small_floating_names']))


if __name__ == '__main__':
    main()
