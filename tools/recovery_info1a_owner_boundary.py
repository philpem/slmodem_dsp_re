#!/usr/bin/env python3
"""Audit original shifted-owner witnesses without inferring an aggregate.

This analysis does not compile or modify reconstruction source. A retained
obj+4 address proves addressing/liveness; it does not prove an embedded type.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def precoder_publications(stream):
    registers, stores = {}, {}
    for line in stream.splitlines():
        instruction = re.search(r'\b(mov|lea)\s+(.+)$', line)
        if not instruction:
            continue
        operation, operands = instruction.groups()
        load = re.fullmatch(r'(0x[0-9a-f]+)\(%ebx\),(%\w+)', operands)
        if load:
            registers[load[2]] = ('address' if operation == 'lea' else 'load', int(load[1], 16))
        zero = re.fullmatch(r'\$0x0,(%\w+)', operands)
        if zero:
            registers[zero[1]] = ('zero', 0)
        store = re.fullmatch(r'(%\w+),(0x[0-9a-f]+)\(%esi\)', operands)
        if store:
            stores[int(store[2], 16)] = registers.get(store[1])
    return stores


def expected_precoder_publications():
    expected = {4: ('address', 0x9c), 0x70: ('zero', 0), 0x74: ('zero', 0)}
    for destination, source, count in [(0x50, 0x6c, 6), (8, 0x84, 6), (0x20, 0x1c, 12)]:
        expected.update({destination + 4*i: ('load', source + 4*i) for i in range(count)})
    return expected


def checks(info, probe, spectral, precoder, echo):
    return {
        'precoder_27_publications': precoder_publications(precoder) == expected_precoder_publications(),
        'echo_unsigned_capacity_guard': bool(re.search(r'11798:.*jb', echo)),
        'echo_SF_publication_and_cursor_reload': all(address + ':' in echo for address in ['117be', '117c3', '117f8']),
        'echo_equality_wrap': bool(re.search(r'117ca:.*je', echo)),
        'echo_bottom_tested_copy': bool(re.search(r'1184b:.*jne', echo)),
        'INFO1a_shifted_address': 'lea    0x4(%esi),%edi' in info,
        'INFO1a_shifted_receiver_uses': all(address + ':' in info and operand in info
            for address, operand in [('8cce', '0x248(%edi),%eax'),
                                     ('8cec', '0x24c(%edi),%ecx'),
                                     ('8dd0', '%ebx,0x248(%edi)')]),
        'INFO1a_full_SI_spill': bool(re.search(r'8dc4:.*mov\s+%eax,0x14\(%esp\)', info)),
        'INFO1a_low_word_use': bool(re.search(r'8fdb:.*movzwl\s+0x14\(%esp\),%edx', info)),
        'probe_independent_shifted_address': 'lea    0x4(%ecx),%eax' in probe,
        'probe_receiver_pair': '0x248(%eax),%edx' in probe and '0x24c(%eax),%eax' in probe,
        'spectral_state_publication_before_Psd': spectral.index('46659:') < spectral.index('4667c:'),
        'spectral_call_and_parameter_reload_order':
            spectral.index('4667c:') < spectral.index('46688:') < spectral.index('46695:')
            < spectral.index('46697:') < spectral.index('466a8:'),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    ap.add_argument('--out-dir', type=Path, default=ROOT/'build/info1a-owner-boundary')
    args = ap.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    symbols = ['V34SetINFO1aBits', 'V34GiveProbeResults', '_ZN19V90SpectralVerifier7processEPfj',
               '_ZN11V92Precoder5resetEP16V92MappingParams',
               '_ZN16V92EchoCanceller17updateEchoHistoryEPfj']
    streams = []
    for symbol in symbols:
        stream = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'),
                                          str(args.reference), symbol], text=True)
        (args.out_dir/(symbol+'.dis')).write_text(stream)
        streams.append(stream)
    verdicts = checks(*streams)
    # A changed spill width must fail even though the consumer remains MOVZWL.
    negative = checks(streams[0].replace('%eax,0x14(%esp)', '%ax,0x14(%esp)'), *streams[1:])
    assert not negative['INFO1a_full_SI_spill'], 'width negative did not fire'
    wrong_field = precoder_publications(streams[3].replace('0x1c(%ebx),%eax', '0x20(%ebx),%eax'))
    assert wrong_field != expected_precoder_publications(), 'field negative did not fire'
    result = {'reference_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
              'symbols': symbols, 'checks': verdicts, 'wrong_width_negative_rejected': True, 'wrong_field_negative_rejected': True,
              'inference_boundary': 'Original RTL unavailable. Shared obj+4 address witnesses do not establish an aggregate type or original spill cause.',
              'source_correspondence': ['v34_object has progress int at +4; receiver fields at +24c/+250.',
                  'V34GiveProbeResults uses int+1 addressing; its F11714 note records two exact address spellings.',
                  'V90SpectralVerifier::process preserves state2 before Psd, Ready call, params reload, optional print and classification.'],
              'next_discriminator': 'A typed callee, constructor or independently bounded member layout establishing an aggregate beginning obj+4; no type or forced-spill controls before it.'}
    (args.out_dir/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Original boundary audit: %d/%d checks; width/field negatives rejected' %
          (sum(verdicts.values()), len(verdicts)))
    assert all(verdicts.values()), verdicts


if __name__ == '__main__':
    main()
