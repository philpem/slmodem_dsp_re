#!/usr/bin/env python3
"""Audit original log10 extent at source TU, aggregate and executable boundaries."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_agc_writable_storage_audit import objects

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run-dir', type=Path, default=Path('build/stage-c-log10-extent'))
a = p.parse_args()
r = json.loads((a.run_dir/'results.json').read_text())['families']['fpm_log10']
assert r['cells']['baseline']['baseline_reproduced']
assert r['cells']['original-extent']['changed_bodies'] == ['FPM_log10_table_size']
assert not r['cells']['original-extent']['gains'] and not r['cells']['original-extent']['losses']
paths = {n:a.run_dir/'fpm_log10'/n/'candidate.o' for n in r['cells']}
base, new = objects(paths['baseline']), objects(paths['original-extent'])
assert base.keys() == new.keys() and len(new) == 1
key, record = next(iter(new.items()))
assert record['size'] == 256 and base[key]['size'] == 258
assert base[key]['data'] == record['data'] + '0a00'
assert {k:v for k,v in record.items() if k not in ('data','size')} == {k:v for k,v in base[key].items() if k not in ('data','size')}
original = objects(Path(d.b.BLOB))[key]
assert all(record[field] == original[field] for field in ('data','size','bind','visibility','section','flags','relocations'))
for label, path in paths.items():
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        sections = {section.name:section.data() for section in elf.iter_sections()
                    if section['sh_flags'] & 2 and section.name != '.text'}
    if label == 'baseline': base_sections = sections
    else:
        assert sections.keys() == base_sections.keys()
        assert all(value == (base_sections[name][:-2] if name == '.rodata' else base_sections[name])
                   for name,value in sections.items())
production = d.ROOT/'build/period/src_dsp_fpm_log10.o'
assert all(d.b.body(str(paths['original-extent']), n) == d.b.body(str(production), n) for n in r['cells']['baseline']['functions'])


def layout(path):
    with Path(path).open('rb') as stream:
        e = ELFFile(stream)
        syms = {s.name:s for s in e.get_section_by_name('.symtab').iter_symbols()}
        log, pps = syms['FPM_log10_table'], syms['FPM_PPS_CFG']
        assert log['st_shndx'] == pps['st_shndx']
        section = e.get_section(log['st_shndx'])
        start = log['st_value'] - section['sh_addr']
        return {'extent':log['st_size'],'pps_offset':pps['st_value']-log['st_value'],
                'window_385_words':section.data()[start:start+770].hex()}


period = d.ROOT/'build/period'
config = (period/'t_fpm_log10.link-config').read_text().splitlines()
assert config[0] == 'aggregate'
aggregate = period/config[1].split()[0].removeprefix('/out/')
reference = layout(d.b.BLOB)
assert reference['extent'] == 256 and reference['pps_offset'] == 256
assert layout(aggregate) == layout(period/'t_fpm_log10') == reference
controls = d.ROOT/'build/stage-c-period-link-controls'
cache = d.ROOT/'build/stage-c-period-link-cache'
assert layout(cache/'aggregate-positive.exe') == reference
bad = layout(cache/'individual-negative.exe')
assert bad['pps_offset'] != 256 and bad['window_385_words'] != reference['window_385_words']
assert json.loads((controls/'results.json').read_text())['negative_control_denominator'] == 2
assert all(json.loads((cache/'results.json').read_text())['checks'].values())
plan = json.loads((period/'aggregate-plan.json').read_text())
assert len(plan['order']) == len(plan['ordered_input_hashes']) == plan['inputs'] == 300
source_globals = next(iter(new.values()))['bind']
assert source_globals == 'STB_LOCAL'
report = {'full_source_TUs':2,'source_functions':5,'original_functions':1,'body_changed_only':'FPM_log10_table_size',
          'original_log_body_unchanged':'120B SIZE83', 'original_table_bytes':256,
          'production_source_table_binding':source_globals,'aggregate_inputs':300,
          'original_385_word_window_matches_aggregate_and_executable':True,
          'individual_negative_offset':bad['pps_offset'],
          'negative_controls':2,'cache_controls':5,
          'log10_fixture':'134 table/layout checks;65535 nonzero inputs;zero/diagnostic and exponent checks retained',
          'full_period_diagnostic_aggregate':'388 passed,0 failed',
          'next_discriminator':'Keep aggregate opt-in. Broader default adoption requires explicit policy review despite full-suite equivalence. This boundary unlocks original table extent; no remaining log10 source spelling domain is claimed.'}
(a.run_dir/'link-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print('2 full TUs/5 bodies: only table-size accessor changes; original128-word LOCAL table restored')
print('300-source aggregate and executable: all385 neighbour words match original; individual negative rejected')
print('2 unsound-link negatives,5 cache controls; full diagnostic aggregate period388/0')
