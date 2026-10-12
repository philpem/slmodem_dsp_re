#!/usr/bin/env python3
"""Audit supported frame combination, collateral schedule and integration."""
import argparse
import hashlib
import json
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_v34_frame_sequence import shift_groups, buffer_refreshes


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--integrated',action='store_true');args=ap.parse_args()
    root=d.ROOT/'build/v34-frame-retention';family=root/'v34shell'
    cells=json.loads((root/'results.json').read_text())['families']['v34shell']['cells']
    base=family/'baseline/candidate.o';rows={}
    for label,cell in cells.items():
        path=family/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest()==cell['object_hash']
        assert inspect(path)==inspect(base) and allocated(path)==allocated(base)
        assert not cell.get('gains') and not cell.get('losses')
        changed=[n for n in cell['functions'] if masked(path,n)!=masked(base,n)]
        expected={'baseline':[],'put-fixed':['decodeDepth','putFrame'],
                  'get-sequence-buffer':['getFrame'],'both':['decodeDepth','getFrame','putFrame']}[label]
        assert changed==expected
        hasget=label in ('get-sequence-buffer','both');hasput=label in ('put-fixed','both')
        ins=d.b.insns(str(path),'getFrame');put=d.b.insns(str(path),'putFrame')
        calls=[o for m,o in ins if m=='call' and o.startswith('*')]
        assert len(calls)==(6 if hasget else 3)
        assert sum('0xe48(' in o for m,o in ins)==(1 if hasget else 3)
        assert sum(m=='call' and o.startswith('*') for m,o in put)==(18 if hasput else 6)
        if hasget:
            assert all('(%esp)' in o for o in calls)
            assert shift_groups(path)[1]==4
            assert buffer_refreshes(path,'0xe80(')['refresh_within_four_instructions']==[True]*3
            assert ('sub','$0x5c,%esp') in ins
        rows[label]={'target_verdicts':{n:d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(path),n)) for n in ('putFrame','getFrame','decodeDepth')},
                     'canonical_changed_bodies':changed,'exact_common_bodies':sum(v[0]=='EXACT' for v in cell['verdicts'].values())}
    # No interaction changes either recovered function or the known collateral.
    both=family/'both/candidate.o'
    assert masked(both,'getFrame')==masked(family/'get-sequence-buffer/candidate.o','getFrame')
    for name in ('putFrame','decodeDepth'):
        assert masked(both,name)==masked(family/'put-fixed/candidate.o',name)
    if args.integrated:
        assert (d.ROOT/'src/pump/v34/v34shell.c').read_bytes()==(family/'both/v34shell.c').read_bytes()
        assert (d.ROOT/'build/tc_out/src_pump_v34_v34shell.c.o').read_bytes()==both.read_bytes()
    rows['denominator']={'complete_TU_cells':4,'common_body_verdicts':48,'integrated':args.integrated}
    (root/'audit.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('4 complete TUs /48 verdicts; original callback/shift/refresh/frame controls; 9 bystanders/data unchanged, decodeDepth collateral reviewed; 0 exact gains/losses')


if __name__=='__main__':main()
