#!/usr/bin/env python3
"""Audit the bounded metric lifetime cross and four fresh C++ boundaries."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
# tools/dis.py must not shadow the standard library during ELF imports.
_saved_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
from elftools.elf.elffile import ELFFile
sys.path[:] = _saved_path

ROOT = Path(__file__).resolve().parents[1]
SYMBOLS = ['_ZN12V92PreFilter7processEPfS0_', '_ZN9V92Mapper7processEPh',
           '_ZN12V90Equalizer29setLinearEquEdgesFadingParamsEff',
           '_ZNK24V90SpectralShapingFilter9getMetricEPKsj']


def inspect(prefilter, mapper, fading, metric):
    return {
        'prefilter_fixed12': 'cmp    $0xb,%edx' in prefilter and prefilter.count('mov    $0xc,') == 4,
        'prefilter_FIR_then_IIR': prefilter.count('R_386_PC32 _ZN8FloatFIR7process') == 2 and
                                prefilter.count('R_386_PC32 _ZN8FloatIIR7process') == 1,
        'prefilter_copy_words': 'mov    (%ecx,%edx,4),%edi' in prefilter and 'mov    %edi,(%esi,%edx,4)' in prefilter,
        'mapper_signed_count': 'movswl 0x26(%esi),%eax' in mapper,
        'mapper_byte_input_short_accumulator': 'movzbw (%ebx,%ecx,1),%dx' in mapper and 'movswl %ax,%edx' in mapper,
        'mapper_unsigned_mode': 'movzbl 0x2(%esi),%eax' in mapper,
        'mapper_short_truncation': 'fistps 0x2(%esp)' in mapper and bool(re.search(r'51148:.*cwtl', mapper)),
        'fading_lower_unordered_reject': bool(re.search(r'3b239:.*jae', fading)) and bool(re.search(r'3b24a:.*jae', fading)),
        'fading_upper_half_clamps': all(address + ':' in fading for address in ['3b2c8', '3b2d1', '3b2f4', '3b2fd']),
        'fading_unsigned_conversions': fading.count('fistpll') == 2,
        'fading_both_hamming_calls': fading.count('R_386_PC32 _Z7hammingIfEvPT_j') == 2,
        'metric_signed_input': 'filds  (%edx)' in metric,
        'metric_SF_coefficient_slots': 'fstps  0x4(%esp)' in metric and 'fstps  (%esp)' in metric,
        'metric_per_block_reload_edge': bool(re.search(r'33263:.*ja.*33200', metric)),
    }


def nontext(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        return {s.name: hashlib.sha256(s.data()).hexdigest() for s in elf.iter_sections() if s.name != '.text'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=ROOT/'build/spectral-metric-representation')
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    args = ap.parse_args()
    streams = []
    for symbol in SYMBOLS:
        text = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'), str(args.reference), symbol], text=True)
        (args.run_dir/(symbol+'.original.dis')).write_text(text)
        streams.append(text)
    checks = inspect(*streams)
    negative = inspect(streams[0].replace('$0xc,', '$0xb,'), *streams[1:])
    assert not negative['prefilter_fixed12'], 'sample-count negative did not fire'
    wrong_mode = inspect(streams[0], streams[1].replace('movzbl 0x2(%esi)', 'movsbl 0x2(%esi)'), *streams[2:])
    assert not wrong_mode['mapper_unsigned_mode'], 'mode signedness negative did not fire'
    wrong_spill = inspect(*streams[:3], streams[3].replace('fstps', 'fstpt'))
    assert not wrong_spill['metric_SF_coefficient_slots'], 'spill-width negative did not fire'
    matrix = json.loads((args.run_dir/'results.json').read_text())['families']['V90SpectralShapingFilter']
    cells = matrix['cells']
    assert cells['baseline']['baseline_reproduced']
    baseline = nontext(args.run_dir/'V90SpectralShapingFilter/baseline/candidate.o')
    nontext_equal = {}
    for label, cell in cells.items():
        assert cell['verdicts'] == cells['baseline']['verdicts']
        assert cell.get('changed_bodies', []) in ([], [SYMBOLS[3]])
        nontext_equal[label] = nontext(args.run_dir/'V90SpectralShapingFilter'/label/'candidate.o') == baseline
    assert all(nontext_equal.values()), nontext_equal
    assert cells['baseline']['object_hash'] == cells['per-block-coefficients']['object_hash']
    assert cells['float-coefficients']['object_hash'] == cells['per-block-float-coefficients']['object_hash']
    report = {'original_checks': checks, 'wrong_sample_count_negative_rejected': True, 'wrong_mode_negative_rejected': True,
              'wrong_spill_width_negative_rejected': True,
              'matrix_cells': len(cells), 'body_verdicts': sum(len(c['verdicts']) for c in cells.values()),
              'all_nontext_sections_raw_equal_including_symbols_bindings_relocations': nontext_equal,
              'mechanism': 'Per-block XF source starts coefficient loads after loop48 in01RTL;06CSE retains them.08GCSE hoists SFloads194..197.09loop hoists XFextensions200..203. Float01RTL extends SFcoefficients inside innerloop;08GCSE shares extensions outside innerloop,09loop hoists them before outerloop65. Float control still promotes coefficients to XF for mixed-XF recurrence and spills four XF values; neither scope recovers original SF slots.',
              'alias_boundary': 'getMetric writes no object/input state and calls nobody. Valid short input span cannot concurrently mutate float coefficient members; reinterpreting float storage as a short array is not a justified alias fixture.',
              'conclusion': 'No source behavior defect demonstrated in the four selected boundaries. No source candidate adopted; same SIZE5 is not an improvement claim.',
              'next_discriminator': 'Original coefficient SF-memory consumers versus current XF pseudo spill are the remaining boundary. Require an independently supported arithmetic/source-use boundary that keeps these coefficients SF; do not expand declaration/type palette.'}
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('%d/%d original checks; three negatives rejected; %d cells/%d body verdicts; nontext equal' %
          (sum(checks.values()), len(checks), report['matrix_cells'], report['body_verdicts']))
    assert all(checks.values()), checks


if __name__ == '__main__':
    main()
