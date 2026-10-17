#!/usr/bin/env python3
"""Audit the four-cell V32 ownership ladder and its deliberately open SMC owner."""
import argparse
import json
from pathlib import Path
import subprocess
import playbook_small_patterns as driver
from recovery_agc_writable_storage_audit import objects
from recovery_v32_storage_owner import FILES
from storage_layout_audit import inventory


def collection(folder):
    functions, data, bindings = {}, {}, {}
    for name in FILES:
        path = folder / (name + '.o')
        for function in driver.b.sizes(str(path)):
            assert function not in functions
            functions[function] = driver.b.body(str(path), function)
        for (_, symbol), record in objects(path).items():
            assert symbol not in data
            data[symbol] = record
        for line in subprocess.check_output(['nm', '-g', '--defined-only', str(path)], text=True).splitlines():
            fields = line.split()
            assert fields[-1] not in bindings
            bindings[fields[-1]] = fields[-2]
    return functions, data, bindings


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=driver.ROOT / 'build/recovery-v32-storage-owner')
    root = ap.parse_args().run_dir
    labels = tuple(json.loads((root/'results.json').read_text())['cells'])
    assert labels == ('baseline', 'buffer-function-owner', 'control-owner', 'consumer-storage-owner')
    baseline, base_data, base_bindings = collection(root/'baseline')
    assert len(baseline) == 34
    report = {}
    for label in labels:
        functions, data, bindings = collection(root/label)
        assert functions.keys() == baseline.keys() and data == base_data and bindings == base_bindings
        verdicts = {}
        for name, (raw, relocs) in functions.items():
            old, old_rel = baseline[name]
            assert relocs == old_rel, (label, name)
            raw, old = bytearray(raw), bytearray(old)
            for offset in relocs:
                raw[offset:offset+4] = old[offset:offset+4] = b'\0'*4
            assert raw == old, (label, name)
            original = driver.b.body(driver.b.BLOB, name)
            grade = list(driver.b.verdict(*original, *functions[name]))
            assert grade == list(driver.b.verdict(*original, *baseline[name]))
            verdicts[name] = grade
        partial = inventory(root/label/'partial.o')
        positions = {r['name']: r['value'] for r in partial['storage']
                     if r['name'] in ('SMCv32_CFG', 'Control_Flag')}
        expected = {'baseline': (2812,2808,2804), 'buffer-function-owner': (2812,2808,2804),
                    'control-owner': (2840,2836,416), 'consumer-storage-owner': (2836,392,416)}[label]
        assert (partial['nobits'][0]['size'], positions['SMCv32_CFG'], positions['Control_Flag']) == expected
        report[label] = {'verdicts': verdicts, 'exact': sum(v[0]=='EXACT' for v in verdicts.values()),
                         'data_objects': len(data), 'defined_global_bindings': len(bindings),
                         'nobits': partial['nobits'], 'global_positions': positions}
    # Local FILE provenance is valid for these LOCAL symbols, never inferred
    # from a nearest FILE record for a GLOBAL declaration.
    original_data = objects(Path(driver.b.BLOB))
    for name in ('tx_in_internal', 'rx_out_internal'):
        assert ('V32mod.c', name) in original_data
        assert original_data['V32mod.c', name]['size'] == 200
    references = driver.b.body(driver.b.BLOB, 'V32FP_modem')[1]
    local_refs = [v for kind, v in references.values() if kind == 'R_386_32' and v[0]=='symbol' and v[1] in ('tx_in_internal','rx_out_internal')]
    assert len(local_refs) == 4
    output = {'full_TUs': 16, 'function_verdicts': 136, 'cells': report,
              'original_local_buffer_references': local_refs,
              'adopted': 'control-owner',
              'unadopted': 'consumer-storage-owner: sole SMC global consumer does not uniquely establish its original TU',
              'invalid_control': 'initial lexical compilation-manifest link rejected by raw baseline check; corrected to tc_link_manifest',
              'scope': 'restores evidenced local-owner boundary; BSS remains4 bytes above original, no padding added'}
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('16 full TUs / 136 verdicts: all34 instruction/relocation bodies and17 exact matches preserved')
    print('all named object bytes/pointer targets/bindings preserved; LOCAL-buffer FILE/4reference witnesses pass')
    print('adopt control-owner: BSS2840 vs original2836; SMC consumer-placement cell matches extent but stays diagnostic')


if __name__ == '__main__':
    main()
