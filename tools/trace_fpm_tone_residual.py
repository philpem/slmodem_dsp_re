#!/usr/bin/env python3
"""Check the adopted tone object's existing four-cell RTL trace; never compile."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_STAGES = set('''01.rtl 02.sibling 04.jump 05.null 06.cse 07.addressof
08.gcse 09.loop 10.bypass 11.cfg 12.bp 14.ce1 16.loop2 17.web 18.cse2
19.life 20.combine 21.ce2 22.regmove 24.lreg 25.greg 26.postreload
27.flow2 28.peephole2 29.ce3 30.rnreg 31.bbro 33.sched2 34.stack 35.mach'''.split())


def require(condition, message):
    # Validation must remain active when Python is invoked with -O.
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', type=Path, required=True,
                        help='completed recovery_fpm_tone_ring_count.py output directory')
    parser.add_argument('--canonical', type=Path,
                        default=ROOT / 'build/tc_repro/src_dsp_fpm_tone.c.o')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = json.loads((args.replay / 'results.json').read_text())
    selected = report['cells']['both']
    require(len(selected['rows']) == 11, 'complete eleven-body ledger required')
    require(selected['changed_bodies'] == ['FPM_TONE_filter'], 'unexpected changed bodies')
    body = args.replay / 'both'
    require((body / 'fpm_tone.c').read_bytes() ==
            (ROOT / 'src/dsp/fpm_tone.c').read_bytes(), 'adopted source mismatch')
    raw = (body / 'fpm_tone.o').read_bytes()
    require(raw == args.canonical.read_bytes(), 'canonical whole-object mismatch')
    require(hashlib.sha256(raw).hexdigest() == selected['object_sha256'],
            'replay object hash mismatch')
    stages = {}
    for path in sorted(body.glob('fpm_tone.c.*')):
        text = path.read_text(errors='replace')
        marker = ';; Function FPM_TONE_filter'
        if marker not in text:
            continue
        start = text.index(marker)
        end = text.find(';; Function ', start + len(marker))
        text = text[start:end if end >= 0 else None]
        chunks = [x for x in text.split('\n\n')
                  if re.match(r'^\(insn(?:[/:[A-Z]+|:[A-Z]+)?\s', x)]
        # GCSE diagnostic prose repeats insns. Count each emitted UID once.
        chunks = list({re.match(r'^\(insn[^ ]* (\d+)', x).group(1): x
                       for x in chunks}.values())
        roots = [x for x in chunks if re.search(
            r'\(set \(reg[^\n]*\n\s*\(mem/f:SI', x) and 'state+0 S4' in x]
        fields = [x for x in chunks if '(set (reg' in x and any(
            y in x for y in ('.kernel+0', '.history+0', '.cfg.len+0', '.hist_idx+0'))]
        stages[path.name] = dict(root_load_insns=len(roots), field_capture_insns=len(fields),
                                 root_loads=roots, field_captures=fields)
    actual = {name.removeprefix('fpm_tone.c.') for name in stages}
    require(actual == EXPECTED_STAGES,
            f'expected 30 named RTL dumps; missing={sorted(EXPECTED_STAGES - actual)}, '
            f'unexpected={sorted(actual - EXPECTED_STAGES)}')
    for name, stage in stages.items():
        number = int(name.split('.')[-2])
        expected = 1 if number < 25 else 5
        require(stage['root_load_insns'] == expected,
                f'{name}: state loads {stage["root_load_insns"]}, expected {expected}')
        require(stage['field_capture_insns'] == 4,
                f'{name}: field captures {stage["field_capture_insns"]}, expected 4')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(dict(scope='reconstruction RTL only; original IR unavailable',
        bodies=11, raw_canonical_identity=True, stages=stages), indent=2) + '\n')
    print('PASS: 11-body ledger, whole-object identity, 30 RTL dumps; first extra root loads: 25.greg')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit('Trace mismatch: ' + str(error))
