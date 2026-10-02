#!/usr/bin/env python3
"""Three staged bit-packer next-position conversion-boundary controls."""
import playbook_small_patterns as driver
from playbook_cid_pack import variants as pack_variants


def variants(path, source):
    parent = pack_variants(path, source)['both']
    start, end, fn = driver.function(parent, 'pack_next_bit')
    old = 'short pos = cid->pack_pos;'
    assert fn.count(old) == 1
    wide = fn.replace(old, 'int pos = cid->pack_pos;')
    use = wide.replace('pos = (short)(pos + 1);', 'pos++;')
    use = use.replace('if (pos == 8)', 'if ((short)pos == 8)')
    return {'baseline': parent,
            'int-position': parent[:start] + wide + parent[end:],
            'position-use-narrowing': parent[:start] + use + parent[end:]}


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-cid-pack-position'
    driver.SOURCE_PATHS = ('src/service/Rxcid.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'Rxcid'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-cid-pack/Rxcid/both/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
