#!/usr/bin/env python3
"""Audit the two-cell defaults publication control without scoring by size."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
_script = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
sys.path[:] = _script
import playbook_small_patterns as replay
from gcc3_value_carriers_audit import inspect
import numeric_consumer_audit as numeric
import v90_default_store_audit as stores
from v90_default_boundary_reproduce import SOURCE, variants

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=ROOT/'build/default-boundary-control')
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    args = ap.parse_args()
    result = json.loads((args.run_dir/'results.json').read_text())
    family = result['families']['V90Parameters']
    assert set(family['cells']) == {'baseline', 'mask-before-defaults'}
    assert family['cells']['baseline']['baseline_reproduced']
    for path, digest in result['headers'].items():
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == digest
    paths = {label: args.run_dir/'V90Parameters'/label/'candidate.o' for label in family['cells']}
    texts = {label: (p.parent/'V90Parameters.cpp').read_text() for label, p in paths.items()}
    assert variants(SOURCE, texts['baseline'])['mask-before-defaults'] == texts['mask-before-defaults']
    assert (ROOT/SOURCE).read_text() == texts['mask-before-defaults'], 'retained source is not the audited control'
    for label, cell in family['cells'].items():
        assert cell['compile_exit'] == 0
        assert hashlib.sha256(paths[label].read_bytes()).hexdigest() == cell['object_hash']
        assert '-DDSPLIB_REPRODUCE_BUGS' in cell['command'][-1]
    metadata = {label: inspect(path) for label, path in paths.items()}
    base, candidate = metadata['baseline'], metadata['mask-before-defaults']
    assert base['records'] == candidate['records']
    assert base['nobits'] == candidate['nobits']
    assert base['relocations'] == candidate['relocations']
    changed_data = sorted(n for n in base['allocated'].keys() | candidate['allocated'].keys()
                          if base['allocated'].get(n) != candidate['allocated'].get(n))
    assert changed_data == ['.rodata.cst4']
    canonical = {name: replay.b.verdict(*replay.b.body(str(paths['baseline']), name),
                                      *replay.b.body(str(paths['mask-before-defaults']), name))
                 for name in family['cells']['baseline']['functions']}
    assert len(canonical) == 9
    assert [n for n, verdict in canonical.items() if verdict[0] != 'EXACT'] == [stores.TARGET]
    assert not family['cells']['mask-before-defaults']['gains']
    assert not family['cells']['mask-before-defaults']['losses']
    maps = {label: stores.trace(path) for label, path in [('reference', args.reference), *paths.items()]}
    comparisons = {label: stores.compare(maps['reference'], maps[label]) for label in paths}
    for label, comparison in comparisons.items():
        assert comparison['call_inventory_and_direction_equal']
        assert comparison['shared_store_sites'] == 340
        assert len(comparison['equal_bytes']) == 338
        assert len(comparison['unresolved_values']) == 2
        assert not comparison['different_bytes']
        assert not comparison['reference_only_sites'] and not comparison['candidate_only_sites']
        assert not maps[label]['unsupported']
        assert not maps[label]['denominator']['unresolved_addresses']
    assert len(comparisons['baseline']['call_relative_order_changes']) == 319
    assert not comparisons['mask-before-defaults']['call_relative_order_changes']
    before = numeric.inventory(paths['baseline'])
    after = numeric.inventory(paths['mask-before-defaults'])
    assert before[0]['_ZN13V90Parameters19loadModemParamsDataEv'] == after[0]['_ZN13V90Parameters19loadModemParamsDataEv']
    report = {'source_revision': result['revision'], 'source_sha256': {label: hashlib.sha256(text.encode()).hexdigest() for label, text in texts.items()},
              'raw_baseline_reproduced': True, 'cells': 2, 'header_hashes_verified': len(result['headers']),
              'canonical_full_TU_comparisons': canonical, 'canonical_bystanders_equal': 8,
              'all_symbols_binding_visibility_equal': True, 'BSS_nontext_relocations_equal': True,
              'changed_data_sections': changed_data,
              'numeric_pool_bytes': {label: m['allocated']['.rodata.cst4'] for label, m in metadata.items()},
              'loadModemParamsData_numeric_values_and_canonical_body_equal': True,
              'reference_comparisons': comparisons,
              'limits': 'Restores original call-relative publication boundary only. All 338 constant stores agree before and after; two dynamic values remain outside this static proof. Full static store order and whole function remain nonexact. No demonstrated runtime defect or public modem reachability claim.'}
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('2 valid full-TU cells; raw baseline reproduced; %d header hashes verified' % len(result['headers']))
    print('8/8 canonical bystanders unchanged; all bindings, BSS and nontext relocations unchanged; .rodata.cst4 materialization changes disclosed')
    print('340/340 stores mapped: 338 constants equal, 2 dynamic; 319 -> 0 diagnostic-relative store differences; exact set remains 6/9')


if __name__ == '__main__':
    main()
