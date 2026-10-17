#!/usr/bin/env python3
"""Replay raw full-TU controls for the Dialer/V92 reset diagnostic path audit."""
import json
from pathlib import Path
import playbook_small_patterns as d
from recovery_literal_storage_audit import inventory


def variants(path, source):
    return {'baseline': source}


if __name__ == '__main__':
    d.REV = '79000775'
    d.SOURCE_PATHS = ('src/dialer/Dialer.c', 'src/pump/v90/V92Phase3Modulator.cpp')
    d.OUT_NAME = 'stage-c-diagnostic-paths-control'
    d.DUMP_FLAGS = ('-v', '-save-temps')
    d.variants = variants
    d.main()
    root = d.ROOT/'build'/d.OUT_NAME
    result = json.loads((root/'results.json').read_text())
    reference, _ = inventory(Path(d.b.BLOB))
    report = {'revision':d.REV,'families':{},'source_changes':False}
    for family, symbol, needles in (
        ('Dialer','DialerProgress',('Samples left =','Consequitive-Commas')),
        ('V92Phase3Modulator','_ZN18V92Phase3Modulator5resetEs23V92Phase3ModulatorStatejP5V92JaPK19tagV90DILdescriptorj',('TRN1u state length',))):
        cell = result['families'][family]['cells']['baseline']
        assert cell['baseline_reproduced']
        candidate = root/family/'baseline/candidate.o'
        current, _ = inventory(candidate)
        original_rows = [r for r in reference[symbol] if any(n in r['text'] for n in needles)]
        current_rows = [r for r in current[symbol] if any(n in r['text'] for n in needles)]
        report['families'][family] = {'full_TU_raw_reproduced':True,'functions':len(cell['functions']),
            'exact_names':sorted(n for n,v in cell['verdicts'].items() if v[0]=='EXACT'),
            'original_consumers':original_rows,'current_consumers':current_rows,'object_hash':cell['object_hash']}
    report['witnesses'] = {
        'Dialer':'Original comma sites7b781/7ba09/7bce2 signed-cap repeat*pause then*8000, publish silence+bc before debug>1, reload as argument; tone sites7bd15/7bd3b/7bdcb receive duration*8 published+bc before print. Source begin_next shares setup across entry/gap/final-digit paths. Static site count3vs2 does not imply a missing runtime diagnostic.',
        'V92_reset':'Original16c07 unsigned comparison against8159; floorstore16c12 versus computedstore16ca2 to+10; both reload+10 and print once16c27/16cb3 before eventCode+14 and codeLevel+4 reset. Source common print preserves this publication and ungated call boundary.',
        'fixtures':'DialerCreate/public progress histories at levels1,2,3: initial tone, postgap tone, initial/postgap/postfinal comma and uncapped comma. V92 real constructor->reset->generateSymbol->reset histories with positive durations4000+(4147,4148,4159,4160), levels0..2; expected8160,8160,8160,8172.',
        'negative_controls':'Temporarily substitute an actual captured argument byte (560->660, and an encoded byte belonging to the final reset length digit); transcript_exact must reject; restore and require equality. No source verdict is suppressed.',
        'next_discriminator':'Guard/path callback disagreement on another boundary with valid component history; do not duplicate static print sites or tune GCC to equalise their counts.'}
    (root/'witness.json').write_text(json.dumps(report,indent=2)+'\n')
    print('2 raw-identical complete TU controls; static diagnostics explained by shared source paths')
