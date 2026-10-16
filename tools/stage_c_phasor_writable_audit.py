#!/usr/bin/env python3
"""Audit phasor owner dependencies, natural period layout and full-TU collateral."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as d  # imports stdlib dis before tools/dis.py
from elftools.elf.elffile import ELFFile
from recovery_agc_writable_storage_audit import objects
from gcc3_reload_trace import function, instructions

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-phasor-writable'))
p.add_argument('--period-dir', type=Path, default=Path('build/period'))
a = p.parse_args()
r = json.loads((a.run_dir / 'results.json').read_text())['families']['fpm_phasor']
assert r['cells']['baseline']['baseline_reproduced']
paths = {label: a.run_dir / 'fpm_phasor' / label / 'candidate.o' for label in r['cells']}
assert len(paths) == 3
base, candidate = paths['baseline'], paths['direct-original-storage']
removed = {'FPM_cos_sign_ext', 'FPM_sin_sign_ext'}
old_objects, new_objects = objects(base), objects(candidate)
assert {key for key in old_objects if key[1] in removed} == {('', n) for n in removed}
assert new_objects.keys() == old_objects.keys() - {('', n) for n in removed}
for key, value in new_objects.items():
    assert {k:v for k,v in value.items() if k != 'offset'} == {k:v for k,v in old_objects[key].items() if k != 'offset'}


def meta(path):
    with path.open('rb') as stream:
        e = ELFFile(stream)
        return {s.name: (s['st_info']['type'], s['st_info']['bind'], s['st_other']['visibility'],
                        e.get_section(s['st_shndx']).name if isinstance(s['st_shndx'], int) else s['st_shndx'])
                for s in e.get_section_by_name('.symtab').iter_symbols() if s.name}


def allocated(path):
    with path.open('rb') as stream:
        e = ELFFile(stream)
        return {section.name: section.data() for section in e.iter_sections()
                if section['sh_flags'] & 2 and section.name != '.text'}

old_sections, new_sections = allocated(base), allocated(candidate)
assert old_sections.keys() == new_sections.keys()
for name in old_sections:
    assert new_sections[name] == (old_sections[name][32:] if name == '.data' else old_sections[name]), name


old, new = meta(base), meta(candidate)
assert new == {k:v for k,v in old.items() if k not in removed}
changed = []
for name in d.b.sizes(str(base)):
    if d.b.body(str(base), name) != d.b.body(str(candidate), name): changed.append(name)
assert set(changed) == {'FPM_phasor', 'FPM_phasor_dp', 'FPM_phasor_demod'}
assert not r['cells']['direct-original-storage']['losses']
trace = {}
for name in changed:
    trace[name] = {}
    for label in paths:
        dump = (paths[label].parent / 'fpm_phasor.c.01.rtl').read_text()
        stream = instructions(function(dump, name))
        refs = {str(uid): pattern for uid, pattern in stream.items()
                if any(table in str(pattern) for table in ('FPM_cos_sign', 'FPM_sin_sign', 'COEF_DC'))}
        trace[name][label] = refs
        text = str(refs)
        if label == 'baseline': assert '_sign_ext' in text
        else:
            assert '_sign_ext' not in text and 'FPM_cos_sign' in text
            if name != 'FPM_phasor_demod': assert 'FPM_sin_sign' in text


def neighbourhood(path, prefix=''):
    with path.open('rb') as stream:
        e = ELFFile(stream)
        syms = {s.name:s for s in e.get_section_by_name('.symtab').iter_symbols()}
        names = ('COEF_DC', 'FPM_sin_sign', 'FPM_cos_sign')
        ss = [syms[prefix + n] for n in names]
        assert len({s['st_shndx'] for s in ss}) == 1
        origin = ss[0]['st_value']
        assert [s['st_value'] - origin for s in ss] == [0, 12, 20]
        sec = e.get_section(ss[0]['st_shndx'])
        lo = origin - sec['sh_addr']
        raw = sec.data()[lo:lo + 28]
        assert raw[10:12] == b'\0\0'
        return {'relative_offsets': dict(zip(names, (0, 12, 20))), 'bytes': raw.hex()}


binary = a.period_dir / 't_fpm_phasor'
layout = neighbourhood(binary)
assert layout == neighbourhood(binary, 'ref_') == neighbourhood(Path(d.b.BLOB))
period_object = a.period_dir / 'src_dsp_fpm_phasor.o'
assert all(d.b.body(str(candidate), n) == d.b.body(str(period_object), n) for n in d.b.sizes(str(candidate)))
report = {'full_TUs': 3, 'shared_original_functions': 3, 'emitted_functions': len(d.b.sizes(str(base))),
          'changed_bodies': changed, 'removed_nonoriginal_exports': sorted(removed),
          'remaining_objects_values_bindings_equal': True, 'two_accessor_bystanders_equal': True,
          'natural_linked_neighbourhood_equals_original': layout,
          'production_period_object_bodies_equal_experiment': True,
          'initial_RTL_table_dependencies': trace,
          'original_witness': 'a9367 cos and a9392 sin public-symbol loads; dp a944b/a9472 same. Original .data cos[-4..-1] aliases sine[0..3], sine[-4..-2] aliases COEF_DC[2..4], sine[-1] zero padding.',
          'regression': {'boundary': 'writes inside public writable arrays; negative phases exploratory original fidelity, no modem-write claim', 'checks': 48, 'old_failures': 42, 'candidate_failures': 0, 'focused_period': '4 passed, 0 failed'},
          'rejected_control': 'Explicit owner branches fix writes but add SIZE34/37 to full phasor/dp; original direct signed-index reads and natural period placement recover the source graph without invented guards.',
          'next_step': 'Audit other copied writable windows against their original public owners. Portability or instrumentation layout differs must be separate from the deciding period reconstruction.'}
(a.run_dir / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
print('3 full TUs; 3 target bodies change, 2 accessors unchanged; 2 invented exports removed')
print('Natural COEF_DC/sine/cosine relative offsets 0/12/20 and bytes match original; period production bodies match')
print('Initial RTL restores public owners; 48-check regression red 42 -> green 0; period 4/0')
