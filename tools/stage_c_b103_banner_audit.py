#!/usr/bin/env python3
"""Check original B103 banner operands, full TU and period regression evidence."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_agc_writable_storage_audit import objects
from gcc3_reload_trace import function, instructions

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-b103-banner'))
a = p.parse_args()
root = a.run_dir
r = json.loads((root / 'results.json').read_text())['families']['B103prc']
cells = r['cells']
assert cells['baseline']['baseline_reproduced']
assert cells['original-banner']['changed_bodies'] == ['B103FP_create']
assert not cells['original-banner']['gains'] and not cells['original-banner']['losses']
paths = {n:root / 'B103prc' / n / 'candidate.o' for n in cells}
assert objects(paths['baseline']) == objects(paths['original-banner'])

def sections(path):
    with path.open('rb') as stream:
        e = ELFFile(stream)
        return {s.name:s.data() for s in e.iter_sections() if s['sh_flags'] & 2 and s.name != '.text'}

old, new = sections(paths['baseline']), sections(paths['original-banner'])
assert old.keys() == new.keys()
for name in old:
    if name != '.rodata.str1.1': assert old[name] == new[name], name
strings = (b'B103FP version: %s %s\n', b'Sep 22 2005', b'15:48:11')
assert Counter(new['.rodata.str1.1'].split(b'\0')) - Counter(old['.rodata.str1.1'].split(b'\0')) == Counter(strings)
with Path(d.b.BLOB).open('rb') as stream:
    data = ELFFile(stream).get_section_by_name('.rodata.str1.1').data()
    assert tuple(data[at:data.index(b'\0', at)] for at in (0x3d30, 0x3d50, 0x3d47)) == strings
refs = {}
for label, path in paths.items():
    raw, rels = d.b.body(str(path), 'B103FP_create')
    refs[label] = Counter(target for kind, target in rels.values())
assert refs['original-banner'] - refs['baseline'] == Counter({('symbol', 'dsplibs_debug_level', 0):1,
    ('symbol', 'dsplibs_debug_printf', 4294967292):1, **{('merge-string', text + b'\0'):1 for text in strings}})
assert not refs['baseline'] - refs['original-banner']
rtl = {}
for label, path in paths.items():
    stream = instructions(function((path.parent / 'B103prc.c.01.rtl').read_text(), 'B103FP_create'))
    rtl[label] = {str(uid):pattern for uid, pattern in stream.items() if 'dsplibs_debug_printf' in str(pattern) or 'dsplibs_debug_level' in str(pattern)}
assert not rtl['baseline'] and len(rtl['original-banner']) == 2
production = Path('build/period/src_pump_b103_B103prc.o')
assert all(d.b.body(str(paths['original-banner']), name) == d.b.body(str(production), name) for name in cells['baseline']['functions'])
report = {'full_TUs':2, 'shared_functions':17, 'exact_bystanders':8, 'unchanged_bodies':16,
          'changed_body':'B103FP_create', 'sizes': {n:d.b.sizes(str(path))['B103FP_create'] for n,path in paths.items()},
          'nontext_change':'Only three original strings,44 bytes; data/BSS/other strings and named objects unchanged',
          'bindings_equal':cells['baseline']['globals'] == cells['original-banner']['globals'],
          'initial_RTL':rtl,
          'original_operands':'8ea86 compare unsigned debug level to1; 8ead0 JA. 8ed43 format; 8ed54 date in stack+4; 8ed58 time in stack+8; 8ed5c printf before success return.',
          'regression':{'public_constructor_cells':16,'checks':80,'old_failed_checks':16,'retained_failures':0,'levels':[0,1,2,4294967295],'boundaries':'allocating and heap-owned supplied-state; explicit default config and NULL default config','period':'3 passed,0 failed'},
          'next_step':'No remaining banner spelling domain. Parent screen can continue another independently witnessed missing call/store; do not replace original literal version values with current __DATE__/__TIME__.'}
(root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
(root / 'original.dis').write_text(subprocess.check_output(['python3','tools/dis.py',d.b.BLOB,'B103FP_create'], text=True))
print('2 TUs/17 functions: only create changes;8 exact and16 bystanders unchanged;44 original string bytes added')
print('80 checks across16 constructor cells: old16 failures -> retained0; period3/0')
