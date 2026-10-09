#!/usr/bin/env python3
"""Verify the recovered default argument and its complete production TU."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def argument_shape(path):
    rows = d.b.insns(str(path), 'FAX_class1_command')
    stores = [args for op,args in rows if op == 'mov' and args.endswith(',0xc(%esp)')]
    assert len(stores) == 1
    recovered = stores == ['%ebp,0xc(%esp)']
    if recovered:
        writes = [(op,args) for op,args in rows if args.endswith(',%ebp')]
        assert writes.count(('xor', '%ebp,%ebp')) == 1
        assert writes.count(('mov', '$0x50,%ebp')) == 1
        assert all((op,args) in [('xor','%ebp,%ebp'), ('mov','$0x50,%ebp'),
                                ('mov','0x28(%esp),%ebp')] for op,args in writes)
    else:
        assert stores == ['%ecx,0xc(%esp)']
        assert ('mov','$0x50,%ecx') in rows
    return {'fourth_argument_store': stores[0], 'original_default_and_ftm_carrier': recovered}

def main():
    root = d.ROOT
    production = root/'build/tc_out/src_fax_fax.c.o'
    package = root/'build/fax-service-values/fax'
    baseline = package/'baseline/candidate.o'
    candidate = package/'extra-1-mode-0/candidate.o'
    assert production.read_bytes() == candidate.read_bytes(), 'production drift from audited complete-TU control'
    source = (root/'src/fax/fax.c').read_text()
    fn = d.function(source, 'FAX_class1_command')[2]
    assert fn.count('int extra = 0;') == 1 and fn.count('extra = 0x50;') == 1
    names = sorted(d.b.sizes(str(production)))
    assert len(names) == 4
    changed = [n for n in names if d.b.body(str(production), n) != d.b.body(str(baseline), n)]
    assert changed == ['FAX_class1_command']
    before = {n: d.b.verdict(*d.b.body(d.b.BLOB,n), *d.b.body(str(baseline),n)) for n in names}
    after = {n: d.b.verdict(*d.b.body(d.b.BLOB,n), *d.b.body(str(production),n)) for n in names}
    assert before['FAX_class1_command'] == ('SIZE',8) and after['FAX_class1_command'] == ('SIZE',5)
    assert all(after[n] == before[n] for n in names if n != 'FAX_class1_command')
    shapes = {label: argument_shape(path) for label,path in [('original',Path(d.b.BLOB)), ('baseline',baseline), ('production',production)]}
    assert shapes['original'] == shapes['production'] and not shapes['baseline']['original_default_and_ftm_carrier']
    report = {'source_sha256': digest(root/'src/fax/fax.c'),
              'production_sha256': digest(production), 'audited_candidate_sha256': digest(candidate),
              'complete_tu_raw_equal_to_audited_control': True, 'body_grades': 4,
              'changed_bodies': changed, 'before': before, 'after': after,
              'argument_shapes': shapes, 'strict_gains': 0, 'strict_losses': 0}
    (root/'build/fax-extra-production-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('3 argument controls / 4 production body grades: original carrier recovered, 3 bystanders fixed; no strict gain/loss')

if __name__ == '__main__':
    main()
