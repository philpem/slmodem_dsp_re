#!/usr/bin/env python3
"""Record unchanged V8 argument/count lifetimes; do not infer original RTL."""
import argparse
import hashlib
import json
from pathlib import Path
from gcc3_reload_trace import function, instructions

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-v8-lifetime'))
a = p.parse_args()
r = json.loads((a.run_dir / 'results.json').read_text())
c = r['families']['V8Interface']['cells']['baseline']
assert c['baseline_reproduced'] and c['compile_exit'] == 0
base = a.run_dir / 'V8Interface' / 'baseline'
assert hashlib.sha256((base / 'candidate.o').read_bytes()).hexdigest() == r['families']['V8Interface']['retained_hash']
rows = {}
for stage in ('01.rtl', '19.life', '20.combine', '24.lreg', '25.greg', '28.peephole2', '30.rnreg', '33.sched2'):
    text = function((base / ('V8Interface.c.' + stage)).read_text(), 'V8GetMessage')
    stream = instructions(text)
    rows[stage] = {str(uid): stream[uid] for uid in (4, 41, 42, 98) if uid in stream}
assert rows['01.rtl']['4'][1][1] == '60'
assert 'sign_extend:SI' in str(rows['01.rtl']['42'])
assert 'sign_extend:SI' in str(rows['20.combine']['42'])
for stage in ('25.greg', '28.peephole2', '30.rnreg', '33.sched2'):
    assert rows[stage]['4'][1][1] == '6'
    assert '6' in str(rows[stage]['98'])
report = {
    'denominator': {'unchanged_full_TUs': 1, 'shared_functions': len(c['functions']), 'exact_functions': sum(v[0] == 'EXACT' for v in c['verdicts'].values()), 'source_variants': 0},
    'raw_object_identical': True,
    'full_TU_verdicts': c['verdicts'],
    'trace': rows,
    'original_witness': {'count': '74c8c MOVZWL wordidx; 74c90 TEST AX; 74c93 JLE; 74c99 CWTL', 'out': '74cc1 reload incoming out from SP+0x24 after byte-only charFlip call'},
    'diagnosis': 'Count promotion exists at initial RTL42; combine folds load41 into42. Out pseudo60 exists at initial RTL4 and crosses the call in store98. Global allocation chooses hard6/EBP; subsequent peephole2/rnreg/sched2 retain it. This is the first observed concrete allocation choice, not a claim about unavailable original compiler dumps.',
    'scope': 'No changed source, behavioural defect, new differential fixture, or source gain. Complete raw-object identity also proves unchanged bystanders, data and symbol bindings.',
    'next_discriminator': 'Close output reload/source-owner hypothesis: original reload names the by-value argument home, and charFlip receives only a byte. Reopen only upon an independently witnessed TU/callee visibility or typed carrier difference that changes the out/rc/n interference graph; count/guard/capture/owner spelling palettes are closed.'
}
(a.run_dir / 'lifetime-audit.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report['denominator']))
print('raw-object control identical; 8 stage snapshots; source lifetime hypothesis closed')
