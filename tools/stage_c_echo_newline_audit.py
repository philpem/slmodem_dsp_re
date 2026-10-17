#!/usr/bin/env python3
"""Audit echo literal recovery, whole-TU collateral and red/green evidence."""
from collections import Counter
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_agc_writable_storage_audit import objects

root = Path('build/stage-c-echo-newline')
r = json.loads((root/'results.json').read_text())['families']['VpcmFloModem']
a = root/'VpcmFloModem/baseline/candidate.o'
b = root/'VpcmFloModem/original-newlines/candidate.o'
symbol = '_ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_'
assert r['cells']['baseline']['baseline_reproduced']
c = r['cells']['original-newlines']
assert set(c['changed_bodies']) == {symbol, 'VPCMXF_Create', '_ZN12VPcmFloModem13getUinfoValueEs'}
for name in d.b.sizes(str(a)):
    if name != symbol:
        x,y = d.b.body(str(a),name), d.b.body(str(b),name)
        assert x == y or d.b.verdict(*x,*y)[0] == 'EXACT', name
assert not c['gains'] and not c['losses']
assert len(d.b.sizes(str(a))) == len(d.b.sizes(str(b))) == 34
assert objects(a) == objects(b)
with a.open('rb') as fa, b.open('rb') as fb:
    ea, eb = ELFFile(fa), ELFFile(fb)
    nontext_relocs = {s.name:s.data() for s in ea.iter_sections()
                      if s['sh_type'] in ('SHT_REL','SHT_RELA') and s.name != '.rel.text'}
    assert nontext_relocs == {s.name:s.data() for s in eb.iter_sections()
                      if s['sh_type'] in ('SHT_REL','SHT_RELA') and s.name != '.rel.text'}


def allocated(path):
    with path.open('rb') as f:
        e = ELFFile(f)
        return {s.name:s.data() for s in e.iter_sections() if s['sh_flags'] & 2 and s.name != '.text'}
old, new = allocated(a), allocated(b)
assert old.keys() == new.keys()
for name in old:
    if name != '.rodata.str1.1': assert old[name] == new[name], name
removed = tuple((' *** Echo Silence '+m+' *** \n').encode() for m in ('ON','Off'))
added = tuple(b'\n'+s for s in removed)
assert Counter(new['.rodata.str1.1'].split(b'\0')) - Counter(old['.rodata.str1.1'].split(b'\0')) == Counter(added)
assert Counter(old['.rodata.str1.1'].split(b'\0')) - Counter(new['.rodata.str1.1'].split(b'\0')) == Counter(removed)
with Path(d.b.BLOB).open('rb') as f:
    blob = ELFFile(f).get_section_by_name('.rodata.str1.1').data()
    assert tuple(blob[x:blob.index(b'\0', x)] for x in (0x830,0x84c)) == added
rawa, ra = d.b.body(str(a),symbol)
rawb, rb = d.b.body(str(b),symbol)
def mask_relocations(raw, rels):
    result = bytearray(raw)
    for offset in rels:
        result[offset:offset+4] = b'\0'*4
    return result
assert mask_relocations(rawa, ra) == mask_relocations(rawb, rb)
before, after = Counter(ra.values()), Counter(rb.values())
assert after-before == Counter({('R_386_32',('merge-string',s+b'\0')):1 for s in added}), after-before
assert before-after == Counter({('R_386_32',('merge-string',s+b'\0')):1 for s in removed}), before-after
logs = Path('build/stage-c-error-screen')
red = (logs/'echo-call-red.log').read_text()
green = (logs/'echo-green.log').read_text()
assert '2/205749 checks failed' in red and 'period differential: 0 passed, 1 failed' in red
assert 'period differential: 2 passed, 0 failed' in green
report = {'cells':2,'body_comparisons':68,'functions':34,'exact_names_unchanged':sorted(n for n,v in c['verdicts'].items() if v[0]=='EXACT'), 'raw_changed_bodies':c['changed_bodies'],'canonical_changed_bodies':[symbol],'verdict':c['verdicts'][symbol],'target_size':d.b.sizes(str(b))[symbol], 'text_instruction_bytes_equal_after_relocation_mask':True,'data_bindings_other_allocated_sections_equal':True,'canonical_relocation_changes':'Only the two merge-string targets gain their original leading LF; no instruction or call/guard changes.', 'period':'2 passed,0 failed','regression':'six component event/level cells;24echo count/guard checks; old2/205749 failures then passing','fixture_boundary':'Constructor-backed component dispatch; explicit demodulator events, no claim of end-to-end modem reachability. Wrapped debug calls retain individual boundaries and forward text; per-trial formatting-overflow check.'}
(root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
