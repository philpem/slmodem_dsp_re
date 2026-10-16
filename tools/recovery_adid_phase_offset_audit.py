#!/usr/bin/env python3
"""Check ADID phase-offset conversions and its two original producer sites.

The source agrees at these boundaries. This is a read-only operand audit,
not proof of full modem reachability or permission for out-of-array inputs.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYMBOLS = ['_ZN25V90AutoDigitalImpDetector24adjustUinfoToPhaseOffsetEs',
           '_ZN20V90Phase3Demodulator14getV90DecisionEf',
           '_ZN20V90Phase3Demodulator14getV92DecisionEf',
           '_ZN20V90Phase3Demodulator22incrementFramePositionEv',
           '_ZN25V90AutoDigitalImpDetector5resetEh7PcmTypes']


def inspect(leaf, v90, v92, increment, reset):
    return {
        'public_increment_equality_six': bool(re.search(r'20ec8:.*cmp.*\$0x6,%eax', increment)) and
            bool(re.search(r'20ecb:.*je', increment)) and bool(re.search(r'20ed1:.*movl.*\$0x0', increment)),
        'public_reset_copies_byte_unmasked': bool(re.search(r'40320:.*mov.*%al,0xa96b\(%ebx\)', reset)),
        'signed_short_argument': bool(re.search(r'44953:.*movswl.*0x44\(%esp\)', leaf)),
        'unsigned_byte_ucode': bool(re.search(r'44958:.*movzbl.*0xa96b\(%ebp\)', leaf)),
        'short_phase_increment': bool(re.search(r'4497a:.*movswl.*%dx,%edx', leaf)),
        'equality_six_wrap': bool(re.search(r'44994:.*cmp.*\$0x6,%dx', leaf)) and
                             bool(re.search(r'44998:.*setne', leaf)),
        'all_reads_before_publications': leaf.index('44988:') < leaf.index('449ab:') < leaf.index('449cb:'),
        'word_table_stride': bool(re.search(r'449ce:.*add.*\$0x100,%edx', leaf)),
        'V90_caller_short_argument_and_reset': bool(re.search(r'246dc:.*cwtl', v90)) and
            SYMBOLS[0] in v90 and bool(re.search(r'246eb:.*movl.*\$0x0,0x4\(%ebx\)', v90)),
        'V92_caller_short_argument_and_reset': bool(re.search(r'2210b:.*cwtl', v92)) and
            SYMBOLS[0] in v92 and bool(re.search(r'2211a:.*movl.*\$0x0,0x4\(%ebx\)', v92)),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    ap.add_argument('--out-dir', type=Path, default=ROOT/'build/adid-phase-offset-audit')
    args = ap.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    streams = []
    for symbol in SYMBOLS:
        stream = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'), str(args.reference), symbol], text=True)
        (args.out_dir/(symbol+'.dis')).write_text(stream)
        streams.append(stream)
    checks = inspect(*streams)
    negative = inspect(streams[0].replace('movswl', 'movzwl'), *streams[1:])
    assert not negative['signed_short_argument'], 'signedness negative did not fire'
    sources = ['src/pump/v90/V90AutoDigitalImpDetector.cpp', 'src/pump/v90/V90Phase3Demodulator.cpp']
    report = {'checks': checks, 'unsigned_argument_negative_rejected': True,
              'reference_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
              'source_sha256': {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources},
              'producer_boundary': 'Phase3 reset initializes framePosition=0; each original caller advances with equality6 reset, skips zero, passes short(framePosition), then resets zero. Valid caller offsets1..5; leaf rotation0..5.',
              'code_boundary': 'ADID reset copies unsigned char code unmasked. Valid linMapp indexing requires code<128. No arbitrary byte/negative phase fixture is justified.',
              'existing_coverage': 't_v90adid run_signal starts with public reset(code0x2a), accumulates samples, rotates block%6. run_maptransforms offset6/code>=128 cases are synthetic out-of-array exploration, not valid component evidence.',
              'conclusion': 'No source discrepancy demonstrated at selected conversion/publication boundaries; no source variant tested.',
              'next_discriminator': 'If byte recovery resumes, inspect GCC lifetime/allocation of cached unsigned ucode and saved[6] separately from the already matched signed conversions; first establish a differing original use age or owner, not a type palette.'}
    (args.out_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('ADID phase-offset boundary: %d/%d checks; unsigned-argument negative rejected' % (sum(checks.values()), len(checks)))
    assert all(checks.values()), checks


if __name__ == '__main__':
    main()
