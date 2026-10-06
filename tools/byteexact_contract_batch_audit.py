#!/usr/bin/env python3
"""Audit live experiment grades and the two-function production integration."""
import hashlib
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

PACKAGES = ('contract-wrapper-unsigned', 'contract-wrapper-fragment',
            'fse-config-copy', 'tone-reversal-energy', 'fax-framer-publication', 'fax-rx-accumulator',
            'fax-rx-delete-dispatch', 'fax-next-state-publication',
            'fax-unframe-publication', 'services-v22-trained',
            'services-detector-capture', 'services-v32-silent-ring',
            'services-tone-cursor', 'services-voice-count-sum',
            'batch-cpp-iir', 'batch-cpp-sine-entry')
WINNERS = {
    'src_fax_faxvmi.c.o': ('fax-framer-publication/faxvmi/post-callback-framer/candidate.o', 'FAXVMI_process'),
    'src_voice_voice.c.o': ('services-voice-count-sum/voice/detector-plus-handler/candidate.o', 'voice_modem'),
}


def main():
    rows = []
    for package in PACKAGES:
        root = d.ROOT / 'build' / package
        ledger = json.loads((root / 'results.json').read_text())
        assert ledger['revision'] == 'e0052eec'
        for family, content in ledger['families'].items():
            for label, cell in content['cells'].items():
                obj = root / family / label / 'candidate.o'
                assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
                source = obj.parent / content['source_path'].split('/')[-1]
                assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
                assert sorted(d.b.sizes(str(obj))) == cell['functions']
                for name, expected in cell['verdicts'].items():
                    actual = d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))
                    assert list(actual) == expected, (package, label, name, actual)
                rows.append({'package': package, 'label': label,
                             'emitted': len(cell['functions']), 'shared': len(cell['verdicts'])})
    assert len(rows) == 92 and sum(r['emitted'] for r in rows) == 937
    before, after = d.ROOT / 'build/production-before', d.ROOT / 'build/tc_out'
    names = {p.name for p in before.glob('*.o')}
    assert len(names) == 300 and names == {p.name for p in after.glob('*.o')}
    assert (before / '.build-config').read_bytes() == (after / '.build-config').read_bytes()
    changed = {name for name in names if (before/name).read_bytes() != (after/name).read_bytes()}
    assert changed == set(WINNERS), changed
    for name, (winner, symbol) in WINNERS.items():
        obj = after / name
        assert obj.read_bytes() == (d.ROOT / 'build' / winner).read_bytes()
        a, b = inspect(before/name), inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert a[key] == b[key], (name, key)
        bodies = {n for n in a['text_positions'] if d.b.body(str(before/name), n) != d.b.body(str(obj), n)}
        assert bodies == {symbol}, bodies
        assert d.b.verdict(*d.b.body(d.b.BLOB, symbol), *d.b.body(str(obj), symbol)) == ('EXACT', 0)
    old = json.loads((d.ROOT/'build/baseline-byteident.json').read_text())
    new = json.loads((d.ROOT/'build/batch-final-byteident.json').read_text())
    gains = set(new['exact_symbols']) - set(old['exact_symbols'])
    losses = set(old['exact_symbols']) - set(new['exact_symbols'])
    assert gains == {row[1] for row in WINNERS.values()} and not losses
    assert old['compared'] == new['compared'] == 1852
    assert old['exact'] == 1067 and new['exact'] == 1069
    assert new['exact_bytes'] == old['exact_bytes'] + 665 == 116229
    report = {'cells': len(rows), 'emitted_comparisons': sum(r['emitted'] for r in rows),
              'shared_comparisons': sum(r['shared'] for r in rows), 'rows': rows,
              'objects': 300, 'raw_unchanged': 298, 'raw_winner_repeats': 2,
              'gains': sorted(gains), 'losses': [], 'exact': 1069, 'compared': 1852,
              'exact_bytes': 116229, 'gain_original_bytes': 665}
    (d.ROOT/'build/batch-production-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Contract batch:', len(rows), 'valid cells;', report['emitted_comparisons'],
          'emitted and', report['shared_comparisons'], 'live canonical comparisons;',
          '300 production objects/298 raw unchanged/2 raw winners; 1069/1852 exact, +665 bytes, 0 losses')


if __name__ == '__main__':
    main()
