#!/usr/bin/env python3
"""Audit the first complete-TU scratch wave; retain unsupported-domain refusals."""
import argparse
from collections import Counter
import json
from pathlib import Path
from gentoo_peep2_search_audit import validate
from gentoo_peep2_search_reproduce import PINS, sha
import playbook_small_patterns as d


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    inventory=json.loads(args.inventory.read_text())
    result=json.loads((args.input/'results.json').read_text())
    assert not result['source_or_rtl_mutation'] and len(result['controls'])==8
    rows, targets, counts, events_by_label=[], [], Counter(), {}
    for control in result['controls']:
        label=control['control'];folder=args.input/label;saved=Path(control['source_directory'])
        assert sha(args.input/control['compiler'])==PINS[control['compiler']]
        assert sha(d.ROOT/'tools/gentoo_peep2_search_trace.py')==control['observer_sha256']
        hashes={key:sha(path) for key,path in [('raw',folder/'raw.o'),('traced',folder/'traced.o'),('saved',saved/'candidate.o')]}
        assert hashes==control['complete_object_hashes'] and len(set(hashes.values()))==1
        events=json.loads((folder/'events.json').read_text());events_by_label[label]=events
        bad=[e for e in events if e['kind']=='enter' and not(e['mode'] in (11,12) and e['constraint']=='r')]
        row={'control':label,'raw_traced_saved_equal':True,'searches':control['searches'],
             'candidate_visits':control['candidate_visits']}
        counts['raw_equal_controls']+=1
        if bad:
            try:validate(events)
            except AssertionError as error:
                assert str(error)=='outside validated HI/SI general-register domain'
            else:raise AssertionError('unsupported search accepted')
            row['validation']='refused outside HI/SI r domain'
            row['unsupported_searches']=[{k:e[k] for k in ('assembler_name','uid','mode','constraint')} for e in bad]
            counts['model_refusals']+=1
        else:
            groups,matches=validate(events)
            assert len(groups)==control['searches']
            assert sum(len(g['candidates']) for g in groups.values())==control['candidate_visits']
            rejected = [m for m in matches.values() if not m['replacement_returned']]
            row['rejected_replacements'] = rejected
            counts['rejected_replacements'] += len(rejected)
            counts['replacement_attempts'] += len(matches)
            row['validation']='validated'
            counts['validated_controls']+=1
            counts['validated_searches']+=len(groups)
            counts['validated_candidate_visits']+=control['candidate_visits']
            symbols=set(d.b.sizes(str(saved/'candidate.o')))
            for target in inventory['candidates']:
                if target['grade']!='REGALLOC' or target['symbol'] not in symbols:
                    continue
                # Correlate only the actual defining copy under review.
                production=d.ROOT/'build/tc_out'/target['worst_object']
                if sha(production)!=sha(saved/'candidate.o'):
                    continue
                specific=[g for g in groups.values() if g['entry']['assembler_name']==target['symbol']]
                targets.append({'control':label,'symbol':target['symbol'],'demangled':target['demangled'],
                                'searches':len(specific),'events':[{'entry':g['entry'],'return':g['return']} for g in specific]})
        rows.append(row)
    assert events_by_label['v8']==events_by_label['v8-repeat']
    assert counts['validated_controls']==7 and counts['model_refusals']==1
    assert counts['rejected_replacements']==5
    assert any(m['cursor_after']==0 and m['searches'] for row in rows for m in row.get('rejected_replacements', []))
    assert next(t for t in targets if t['symbol']=='_ZN8FloatFIR5resetEv')['searches']==1
    assert next(t for t in targets if t['symbol']=='dp_v8_init')['searches']==1
    args.output.write_text(json.dumps({'revision':inventory['revision'],'counts':dict(counts),'controls':rows,
                                     'register_targets':targets,'independent_v8_repeat_equal':True,
                                     'scope':'current searches only; original allocator state not measured'},indent=2)+'\n')
    print(dict(counts))
    for target in targets:print(target['demangled'],target['control'],target['searches'])


if __name__=='__main__':main()
