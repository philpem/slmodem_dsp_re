#!/usr/bin/env python3
"""Audit full-TU identity and the branch-result allocation preimage."""
import json
from collections import Counter
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks, fingerprint
from gcc3_reload_trace import instructions, register

def branch_results(folder):
    stream = instructions(chunks(folder/'V27_SDM.c.24.lreg', 'SDMv27_init')[0])
    return Counter(register(pattern[1]) for pattern in stream.values()
                   if isinstance(pattern, list) and len(pattern) == 3 and pattern[0] == 'set'
                   and isinstance(pattern[1], list) and pattern[1][0] == 'reg:HI'
                   and isinstance(pattern[2], list) and str(pattern[2][0]).startswith('mem'))

def main():
    root = d.ROOT/'build/sdmv27-common-value/V27_SDM'
    baseline, candidate = root/'baseline/candidate.o', root/'common-value/candidate.o'
    assert inspect(baseline) == inspect(candidate)
    functions = d.b.sizes(str(baseline))
    assert functions == d.b.sizes(str(candidate)) and len(functions) == 3
    changed = [n for n in functions if d.b.body(str(baseline), n) != d.b.body(str(candidate), n)]
    assert changed == ['SDMv27_init']
    assert d.b.verdict(*d.b.body(d.b.BLOB, 'SDMv27_init'), *d.b.body(str(candidate), 'SDMv27_init')) == ('EXACT', 0)
    assert d.b.verdict(*d.b.body(d.b.BLOB, 'SDMv27_init'), *d.b.body(str(baseline), 'SDMv27_init'))[0] != 'EXACT'
    assert branch_results(root/'baseline') == {60: 1, 61: 1}
    assert branch_results(root/'common-value') == {60: 2}
    stages = [instructions(chunks(root/'common-value'/('V27_SDM.c.'+stage), 'SDMv27_init')[0])
              for stage in ('27.flow2', '28.peephole2', '30.rnreg')]
    assert [fingerprint(stream) for stream in stages].count(fingerprint(stages[0])) == 3
    source = (d.ROOT/'src/fax/V27_SDM.c').read_text()
    _, _, fn = d.function(source, 'SDMv27_init')
    assert fn.count('sdm->nbits = cfg != 0 ? cfg->nbits : SDMv27_CFG.nbits;') == 1
    assert fn.index('if (cfg->nbits == 2)') > fn.index('sdm->nbits =')
    report = {'gains': ['SDMv27_init'], 'losses': [], 'exact_original_bytes_added': 89,
              'unchanged_bystanders': 2, 'full_metadata_equal': True,
              'local_branch_results': dict(branch_results(root/'baseline')),
              'common_branch_result': dict(branch_results(root/'common-value')),
              'candidate_flow2_peephole2_rnreg_patterns_equal': True,
              'body_verdicts': {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(candidate), n)) for n in functions}}
    (root.parent/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('SDMv27_init EXACT: 89 bytes; 2 bystanders unchanged; complete metadata equal; HI results 2 -> 1')

if __name__ == '__main__':
    main()
