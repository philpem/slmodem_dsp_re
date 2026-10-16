#!/usr/bin/env python3
"""Trace the saved scalar threshold operands and stack repair without compiling."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks

HEADER = 'int GenericToneDetector::process(float)'
STAGES = ('01.rtl', '04.jump', '20.combine', '24.lreg', '25.greg',
          '26.postreload', '27.flow2', '28.peephole2', '33.sched2', '34.stack', '35.mach')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path,
                        default=driver.ROOT / 'build/recovery-generic-tone-publication')
    root = parser.parse_args().run_dir
    folder = root / 'GenericToneDetector/output-first'
    report = {}
    for stage in STAGES:
        text, = chunks(folder / ('GenericToneDetector.cpp.' + stage), HEADER)
        stream = instructions(text)
        chosen = {str(uid): pattern for uid, pattern in stream.items()
                  if uid in (75, 85, 94, 95, 99, 100, 280, 281, 282, 325, 326, 327, 328)}
        assert 99 in stream and 'threshold' in str(stream[99])
        compare_uid = 100 if stage < '27.flow2' else 280
        assert compare_uid in stream
        condition = str(stream[compare_uid])
        if stage in ('01.rtl', '04.jump', '20.combine', '24.lreg'):
            assert condition.index("'93'") < condition.index("'80'")
        elif stage < '34.stack':
            assert condition.index('threshold') < condition.index('meanOut')
        else:
            assert stream[280][2][2][1][:2] == ['reg:SF', '8']
            assert stream[280][2][2][2][:2] == ['reg:SF', '10']
        order = list(stream)
        report[stage] = {'selected_patterns': chosen,
                         'selected_order': [uid for uid in order if str(uid) in chosen],
                         'threshold_compare_uid': compare_uid}
    before = report['33.sched2']['selected_patterns']
    after = report['34.stack']['selected_patterns']
    assert all(str(uid) not in before and str(uid) in after for uid in (325, 326, 327, 328))
    assert after['327'][0] == 'set' and after['327'][1][:2] == ['reg:DF', '9']
    source = (folder / 'GenericToneDetector.s').read_text()
    assert '280\t*cmpfp_2_sf_1' in source and '327\t*movdf_nointeger' in source
    result = {'stage_streams': len(report), 'stages': report,
              'scope': 'retained compiler evidence; original RTL unavailable',
              'observation': 'threshold remains the first compare operand from expansion through stack conversion; stack pass adds four selected repair UIDs',
              'original_operand_witness': '103e1 FCOMPS member: meanOut versus threshold; FNSTSW/SAHF then 103f5 JAE',
              'remaining_discriminator': 'an original-supported source operand/age graph that yields meanOut first and a folded threshold memory read without boolean materialization, extra narrowing, duplicated arithmetic or late branch-owned stores'}
    (root / 'predicate-stage-trace.json').write_text(json.dumps(result, indent=2) + '\n')
    print('11 stage streams: threshold-first operand preserved; compare UID100 splits to280 by27.flow2')
    print('34.stack introduces repair UIDs325/326/327/328; threshold load99 survives and meanOut needs a separate pop')


if __name__ == '__main__':
    main()
