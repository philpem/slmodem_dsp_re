#!/usr/bin/env python3
"""Two staged comparison use-site controls after ring-result carrier recovery."""
import playbook_small_patterns as driver
from playbook_v32_smc_carrier import variants as carrier_variants
import json
import subprocess


def variants(path, source):
    parent = carrier_variants(path, source)['both']
    old = 'if (next >= limit)'
    assert parent.count(old) == 1
    return {'baseline': parent,
            'short-compare': parent.replace(old, 'if ((short)next >= limit)')}


def audit():
    root = driver.ROOT/'build'
    sources, objects = set(), set()
    count = 0
    reference = root/'playbook-v32-smc-tag/V32SMC_TX/baseline/candidate.o'

    def data(obj):
        rows = subprocess.check_output(
            ['objdump', '-s', '-j', '.data', '-j', '.rodata', str(obj)], text=True)
        return '\n'.join(rows.splitlines()[2:])

    expected_data = data(reference)
    for family in ('tag', 'ifcvt', 'carrier', 'compare'):
        path = root/('playbook-v32-smc-' + family)
        cells = json.loads((path/'results.json').read_text())['families']['V32SMC_TX']['cells']
        for name, cell in cells.items():
            count += 1
            sources.add(cell['source_hash'])
            objects.add(cell['object_hash'])
            obj = path/'V32SMC_TX'/name/'candidate.o'
            assert data(obj) == expected_data
            assert len(cell['functions']) == 3 and len(cell['globals']) == 5
            assert not cell.get('gains') and not cell.get('losses')
            if family == 'ifcvt' and not name.endswith('-conditional'):
                prior = root/'playbook-v32-smc-tag/V32SMC_TX'/name/'candidate.o'
                assert obj.read_bytes() == prior.read_bytes()
    candidate = root/'playbook-v32-smc-compare/V32SMC_TX/short-compare/candidate.o'
    a, ar = driver.b.body(driver.b.BLOB, 'SMCv32_encoder_abs')
    b, br = driver.b.body(str(candidate), 'SMCv32_encoder_abs')
    assert a[0x40:0x84] == b[0x40:0x84]
    assert {k: v for k, v in ar.items() if 0x40 <= k < 0x84} == {
        k: v for k, v in br.items() if 0x40 <= k < 0x84}
    assert count == 24
    result = {'cells': count, 'sources': len(sources), 'objects': len(objects),
              'data_sections_unchanged': True, 'loop_bytes': 68}
    (root/'playbook-v32-smc-compare/audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('V32SMC_TX audit:', result)


if __name__ == '__main__':
    driver.REV = '6eff516d'
    driver.OUT_NAME = 'playbook-v32-smc-compare'
    driver.SOURCE_PATHS = ('src/pump/v32/V32SMC_TX.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'V32SMC_TX'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-v32-smc-carrier/V32SMC_TX/both/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
    audit()
