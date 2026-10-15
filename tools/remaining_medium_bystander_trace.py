#!/usr/bin/env python3
"""Trace the unchanged unframer through checked RTL alias-set bijections."""
import json
from pathlib import Path
from gcc3_alias_stage_audit import compare, controls
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks

ROOT=Path(__file__).resolve().parents[1]


def main():
    root=ROOT/'build/remaining-medium-boundary/faxvmi_hdlc'
    report={}
    count=controls()
    for suffix in ('01.rtl','24.lreg','25.greg','26.postreload',
                   '27.flow2','28.peephole2','30.rnreg','33.sched2'):
        paths=[root/cell/('faxvmi_hdlc.c.'+suffix) for cell in ('baseline','framer-reloads')]
        streams=[instructions(chunks(path,'faxvmi_hdlc_unframe')[0]) for path in paths]
        report[suffix]=compare(*streams)
    assert not report['01.rtl']['differing_uids']
    assert all(not report[stage]['differing_uids'] for stage in ('24.lreg','25.greg','26.postreload','27.flow2'))
    assert report['28.peephole2']['differing_uids']==[1276,1277,1281,1282,1284,1285,1286,1287,1291,1292]
    assert all(report[stage]['differing_uids']==[1291,1292,1293,1294] for stage in ('30.rnreg','33.sched2'))
    output={'observer_controls':count,'retained_stages':report,
            'first_available_instruction_difference':'28.peephole2',
            'limit':'retained source-unchanged function; no original RTL or compiler-state recovery'}
    (ROOT/'build/remaining-medium-bystander-stages.json').write_text(json.dumps(output,indent=2)+'\n')
    print(count,'observer positive/refusal controls; 8 stages; initial RTL through flow2 match, first available difference28.peephole2')


if __name__=='__main__':main()
