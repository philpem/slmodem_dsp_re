#!/usr/bin/env python3
"""Audit the bounded historical constructor-floor replays."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/v90-parameters-ratchet-ab'
TARGET='_ZN13V90ParametersC2EP19_tagModemParameters'

def main():
    current=json.loads((OUT/'results.json').read_text());cells=current['cells'].copy()
    for name in ('history.json','retype-era.json','floor-stock.json'):cells.update(json.loads((OUT/name).read_text()))
    assert len(cells)==8
    metadata={name:inspect(OUT/name/'candidate.o') for name in cells}
    for name,cell in cells.items():
        assert len(cell['functions'])==len(cell['verdicts'])==9
        assert metadata[name]['records']==metadata['gentoo']['records']
        assert metadata[name]['nobits']==metadata['gentoo']['nobits']
        assert metadata[name]['relocations']==metadata['gentoo']['relocations']
    assert current['cells']['gentoo']['raw_baseline_reproduced']
    assert not cells['stock']['changed_bodies'] and metadata['stock']==metadata['gentoo']
    assert cells['pre-434-retype']['verdicts'][TARGET]==['EXACT',0]
    assert cells['post-434-retype']['verdicts'][TARGET]==['BYTES',4]
    changed=[n for n in cells['pre-434-retype']['functions'] if d.b.body(str(OUT/'pre-434-retype/candidate.o'),n)!=d.b.body(str(OUT/'post-434-retype/candidate.o'),n)]
    assert changed==[TARGET]
    assert metadata['pre-434-retype']==metadata['post-434-retype']
    assert cells['floor-tu-floor-profile-stock']['verdicts'][TARGET]==['EXACT',0]
    for name in ('floor-tu-floor-profile','floor-tu-current-profile'):assert cells[name]['verdicts'][TARGET]==['EXACT',0]
    assert cells['current-tu-floor-profile']['verdicts'][TARGET]==['BYTES',4]
    for symbol in cells['floor-tu-floor-profile']['functions']:
        assert d.b.body(str(OUT/'floor-tu-floor-profile/candidate.o'),symbol)==d.b.body(str(OUT/'floor-tu-floor-profile-stock/candidate.o'),symbol)
    assert metadata['floor-tu-floor-profile']==metadata['floor-tu-floor-profile-stock']
    output={'cells':8,'emitted_body_comparisons':72,'raw_current_baseline_reproduced':True,
            'all_symbols_binding_bss_nontext_equal':True,'constructor_only_change_at_2efa968b':True,
            'constructor_grades':{name:cell['verdicts'][TARGET] for name,cell in cells.items()},
            'retype_era_data_metadata_equal':True,'floor_profile_stock_gentoo_full_bodies_metadata_equal':True}
    (OUT/'audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print('8 forensic controls /72 emitted bodies; raw current baseline reproduced; all bindings/BSS/nontext unchanged')
    print('Floor source EXACT under stock/Gentoo; current source BYTES4 under both profiles/compilers')
    print('2efa968b before/after: only C2 changes; all8 bystanders and all data/metadata identical')
if __name__=='__main__':main()
