#!/usr/bin/env python3
"""Trace existing beta mask and count publication; no candidate compilation."""
import argparse,json
from pathlib import Path
import playbook_small_patterns
from gcc3_value_carriers_audit import function_chunk
from gcc3_reload_trace import instructions
ROOT=Path(__file__).resolve().parents[1]

def sets(node):
    if not isinstance(node,list):return []
    if node and node[0]=='set':return [node]
    return sum([sets(x) for x in node],[])

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dumps',type=Path,required=True,help='existing PR279 beta-x87-mode/V90Equalizer dump directory')
    ap.add_argument('--output',type=Path,default=ROOT/'build/beta-shift-publication-trace.json');args=ap.parse_args()
    report={}
    for cell in ('baseline','float-diagnostic','double-diagnostic'):
        report[cell]={}
        for fn,field in [('setLinearEquBeta','linearEquMmxShift'),('setDfeBeta','dfeMmxShift')]:
            trace={}
            for stage in ('01.rtl','21.ce2','22.regmove','35.mach'):
                rows=instructions(function_chunk(args.dumps/cell/('V90Equalizer.cpp.'+stage),'V90Equalizer::'+fn+'('))
                masks=[(u,n) for u,n in rows.items() if 'and:SI' in str(n) and "'31', '[0x1f]'" in str(n)]
                shifts=[(u,n) for u,n in rows.items() if 'ashift:SI' in str(n)]
                stores=[(u,n) for u,n in rows.items() if field in str(n) and "'shift'" in str(n)]
                assert len(masks)==len(shifts)==len(stores)==1
                mask=masks[0];shift=shifts[0];store=stores[0];uids=list(rows)
                assert uids.index(store[0])<uids.index(mask[0])<uids.index(shift[0])
                op=sets(mask[1])[0];source=op[2][1];destination=op[1]
                destructive=source==destination
                assert destructive==(stage in ('22.regmove','35.mach'))
                trace[stage]={'publication':{'uid':store[0],'pattern':store[1]},
                              'mask':{'uid':mask[0],'pattern':mask[1],'destructive':destructive},
                              'shift':{'uid':shift[0],'pattern':shift[1]}}
            report[cell][fn]=trace
    args.output.write_text(json.dumps({'cells':3,'functions_per_cell':2,'streams':24,'traces':report,
                                      'first_observed_mask':'01.rtl','first_destructive_mask_boundary':'21.ce2 ->22.regmove'},indent=2)+'\n')
    print('3 existing cells / 2 setters / 24 streams: AND31 present initially; publication precedes mask and shift')
    print('21.ce2 has distinct count pseudos;22.regmove reuses source count destructively in all6 bodies')
if __name__=='__main__':main()
