#!/usr/bin/env python3
"""Trace ordered CID_process boundaries in an existing static Gentoo fixture.

This observes public-create/feed/delete component histories, not modem end-to-end
reachability. Calls made directly by CID_process are expanded, plus constructor
retuning and the receiver's resampler calls; other receiver internals remain
inside cid_progress. No compilation, input mutation, inferior
calls or production source changes are performed. Debug/TTY return values are
the harness host contract; CID_process itself ignores those int results.
The period-linked fixture carrier is checked against a continuous sine within
integer rounding bounds. Optional cap coverage requires four nonempty fields
and a rendered fifth field, without fixing their payload text.
"""
import playbook_small_patterns as d
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
from collections import Counter
from functools import lru_cache
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
GENTOO = 'GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)'
RUNTIME = 'GCC: (GNU) 3.3.5  (Gentoo Linux 3.3.5-r1, ssp-3.3.2-3, pie-8.7.7.1)'
# Offsets witnessed independently in dis.py; these are native E8 call sites.
SITES = {
    'original': (386, 0xc0, [(0x80, 'progress'), (0xad, 'get_strings'),
        (0xdd, 'strlen'), (0xf3, 'tty'), (0x113, 'tty'), (0x11b, 'strlen'),
        (0x153, 'debug'), (0x167, 'debug'), (0x178, 'debug')]),
    'candidate': (368, 0xb0, [(0x52, 'progress'), (0x9d, 'get_strings'),
        (0xc9, 'strlen'), (0xe1, 'tty'), (0x101, 'tty'), (0x109, 'strlen'),
        (0x140, 'debug'), (0x155, 'debug'), (0x166, 'debug')]),
}
CALL_NAMES = {'progress': 'cid_progress', 'get_strings': 'cid_get_strings',
              'strlen': 'sysdep_strlen', 'tty': 'modem_send_to_tty',
              'debug': 'dsplibs_debug_printf'}
BODY_HASHES = {
    'original': 'fa9b75b0021d2ac9f8f0b538421089ae8938b7b11a84a903e00934bedabc5973',
    'candidate': '88112159537fe25f355f5f2d005419830bcdb0724384e1bd42e1191548d8bc2f',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Object:
    def __init__(self, path):
        self.stream = path.open('rb')
        self.elf = ELFFile(self.stream)
        assert self.elf.elfclass == 32 and self.elf.little_endian
        assert self.elf['e_machine'] == 'EM_386'
        self.symtab = self.elf.get_section_by_name('.symtab')
        self.symbols = {s.name: s for s in self.symtab.iter_symbols() if s.name}
        self.comments = sorted(set(x.decode() for x in self.elf.get_section_by_name('.comment').data().split(b'\0') if x))

    def read(self, address, count):
        sections = [s for s in self.elf.iter_sections() if s['sh_flags'] & 2
                    and s['sh_addr'] <= address < s['sh_addr'] + s['sh_size']]
        assert len(sections) == 1, (address, count)
        section = sections[0]
        offset = address - section['sh_addr']
        assert offset + count <= section['sh_size']
        return section.data()[offset:offset + count]

    def body(self, name):
        sym = self.symbols[name]
        section = self.elf.get_section(sym['st_shndx'])
        start = sym['st_value'] - section['sh_addr']
        code = bytearray(section.data()[start:start + sym['st_size']])
        return sym, code

    def relocations(self, sym):
        result = []
        for section in self.elf.iter_sections():
            if section['sh_type'] != 'SHT_REL' or section['sh_info'] != sym['st_shndx']:
                continue
            syms = self.elf.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                offset = relocation['r_offset'] - sym['st_value']
                if 0 <= offset < sym['st_size']:
                    result.append((offset, relocation['r_info_type'], syms.get_symbol(relocation['r_info_sym'])))
        return result


def verify_body(obj, linked, name, linked_name, kind):
    """Compare every literal byte and independently resolve all 16 relocations."""
    sym, source = obj.body(name)
    lsym, actual = linked.body(linked_name)
    assert len(source) == len(actual)
    relocations = obj.relocations(sym)
    checks = []
    for offset, typ, target in relocations:
        assert typ in (1, 2), 'unreviewed relocation type'
        addend = int.from_bytes(source[offset:offset + 4], 'little', signed=True)
        value = int.from_bytes(actual[offset:offset + 4], 'little')
        resolved = (value + lsym['st_value'] + offset) & 0xffffffff if typ == 2 else value
        if target['st_info']['type'] == 'STT_SECTION':
            assert typ == 1, 'unreviewed section-relative PC relocation'
            section = obj.elf.get_section(target['st_shndx'])
            expected = section.data()[target['st_value'] + addend:]
            expected = expected[:expected.index(0) + 1]
            assert linked.read(resolved, len(expected)) == expected
            description = {'section': section.name, 'string_hex': expected.hex()}
        else:
            target_name = target.name
            if kind == 'original' and 'ref_' + target_name in linked.symbols:
                target_name = 'ref_' + target_name
            # The fixture's transparent host callback wrappers are intentional.
            if target.name == 'modem_send_to_tty':
                target_name = '__wrap_' + ('ref_' if kind == 'original' else '') + target.name
            assert target_name in linked.symbols, target_name
            expected = (linked.symbols[target_name]['st_value'] + addend) & 0xffffffff
            assert resolved == expected, (kind, offset, target_name, hex(resolved), hex(expected))
            description = {'symbol': target_name}
        checks.append({'offset': offset, 'type': typ, 'target': description})
        source[offset:offset + 4] = b'\0' * 4
        actual[offset:offset + 4] = b'\0' * 4
    assert actual == source, (kind, 'linked/object literal bytes differ')
    assert len(relocations) == 16
    return {'bytes': len(source), 'normalized_sha256': hashlib.sha256(source).hexdigest(),
            'relocations_checked': checks, 'address': lsym['st_value']}


def canonical(row):
    return {key: value for key, value in row.items()
            if key not in ('kind', 'input', 'receiver', 'modem', 'strings')}


def check_carrier(values):
    """Check the period-compiled lookup against a continuous sine, within
    half an integer sample for rounding; also check exact carrier symmetry."""
    assert len(values) == 64
    errors = [abs(value - 8000 * math.sin(2 * math.pi * i / 64))
              for i, value in enumerate(values)]
    half_cycle_errors = sum(values[i] != -values[i + 32] for i in range(32))
    proof = {'samples': 64, 'max_quantization_error': max(errors),
             'half_cycle_symmetry_errors': half_cycle_errors,
             'quadrature': [values[i] for i in (0, 16, 32, 48)]}
    assert max(errors) <= 0.500001 and half_cycle_errors == 0, proof
    assert proof['quadrature'] == [0, 8000, 0, -8000], proof
    return proof


@lru_cache(maxsize=32)
def reference_wave(wire_hex, rate, oscillator_rate=None):
    """Independent sample reference from the decoded async wire and line rate.
    The optional oscillator divisor exists only for a known-wrong control."""
    assert rate in (8000, 9600)
    oscillator_rate = oscillator_rate or rate
    bits = [i & 1 for i in range(300)] + [1] * 180
    for value in bytes.fromhex(wire_hex):
        bits += [0] + [(value >> i) & 1 for i in range(8)] + [1]
    bits += [1] * 60
    table = [round(8000 * math.sin(2 * math.pi * i / 64)) for i in range(64)]
    phase, wave = 0, bytearray()
    for i in range(len(bits) * rate // 1200):
        frequency = 1200 if bits[i * 1200 // rate] else 2200
        phase = (phase + ((frequency << 32) + oscillator_rate // 2) // oscillator_rate) & 0xffffffff
        wave.extend(table[phase >> 26].to_bytes(2, 'little', signed=True))
    return bytes(wave)


def validate(trace, require_cap=False, require_raw9600=False):
    assert trace['exit_codes'] == [0] and not trace['errors']
    assert not trace['active'] and not trace['waiting'] and not trace['live']
    records = trace['records']
    assert records
    sides = {kind: [canonical(r) for r in records if r['kind'] == kind]
             for kind in SITES}
    assert sides['original'] == sides['candidate'], 'ordered boundary mismatch'
    assert all(row['finished'] for row in records)
    assert all(isinstance(row['created'], bool) for row in trace['lifecycles'])
    assert all(row.get('deleted') for row in trace['lifecycles'] if row['created'])
    retunes = Counter()
    for row in trace['lifecycles']:
        if not row['created']:
            assert not row['retunes']
            continue
        assert row['fsk_rate'] == row['rate'] and row['mark_conf_step'] == 9
        assert row['retunes'] == ([{'requested': 9600, 'before': 8000}] if row['rate'] == 9600 else [])
        if row['kind'] == 'original':
            retunes[str(row['rate'])] += len(row['retunes'])
    counts = Counter()
    progress_results = Counter()
    chunks = Counter()
    available_fields = Counter()
    delivered_fields = Counter()
    packets = Counter()
    resamplers = Counter()
    success_shapes = Counter()
    for row in sides['original']:
        if row['wire_hex']:
            wave = reference_wave(row['wire_hex'], row['rate'])
            assert row['wave_samples'] == len(wave) // 2
            assert row['wave_sha256'] == hashlib.sha256(wave).hexdigest(), 'captured waveform differs from declared wire/rate'
            counts['waveform_checks'] += 1
        counts['process_calls'] += 1
        counts['debug_level_' + str(row['debug'])] += 1
        events = row['events']
        pos, used, result = 0, 0, 0
        while used < row['count']:
            p = events[pos]
            assert p['call'] == 'progress'
            assert p['input_offset'] == used and p['n_in'] == min(192, row['count'] - used)
            assert p['what'] == 0 and p['n_out'] == 0
            assert p['state_in'][:2] == [0, row['cid_val']] and p['state_out'][:2] == [0, row['cid_val']]
            assert row['rate'] in (8000, 9600) and row['tx_rate'] == row['rate']
            if row['rate'] == 9600:
                assert not p['resampler_counts'], '9600-Hz path unexpectedly resamples'
            else:
                assert all(n == 160 for n in p['resampler_counts'])
            resamplers[str(row['rate'])] += len(p['resampler_counts'])
            progress_results[str(p['result'])] += 1
            chunks[str(p['n_in'])] += 1
            pos += 1
            if not p['result']:
                used += p['n_in']
                continue
            result = 1 if p['result'] == 1 else -1
            packet = bytes.fromhex(p['packet_hex'])
            assert packet == bytes.fromhex(row['wire_hex']), 'received packet differs from serialized fixture wire'
            counts['exact_wire_packets'] += 1
            assert len(packet) >= 3
            frame_length = packet[1] + 3
            checksum_ok = len(packet) >= frame_length and sum(packet[:frame_length]) % 256 == 0
            packets[f'type_{packet[0]:02x}_result_{p["result"]}_checksum_{checksum_ok}'] += 1
            if result == 1:
                assert checksum_ok, 'successful packet has no complete valid wire checksum'
                success_shapes[f'{row["rate"]}/{row["cid_val"]}/{row["debug"]}'] += 1
            if row['debug'] > 1:
                msg = 'cid: CID Success\n' if result == 1 else 'cid: CID Failure\n'
                assert events[pos] == {'call': 'debug', 'format': msg, 'result': 0}
                pos += 1
            if result == 1:
                strings = events[pos]
                assert strings['call'] == 'get_strings' and strings['result_offset'] == 8
                raw = bytes.fromhex(strings['strings_hex'])
                if row['cid_val'] == 2:
                    expected = packet[:-1].hex().encode()
                    assert raw == expected + bytes(600 - len(expected)), 'raw output differs from type/length/payload hex without checksum'
                    counts['raw_wire_output_checks'] += 1
                fields, cursor = 0, 0
                while raw[cursor]:
                    fields += 1
                    cursor = raw.index(0, cursor) + 1
                available_fields[str(fields)] += 1
                pos += 1
                offset, delivered = 0, 0
                for i in range(4):
                    empty = raw[offset] == 0
                    assert events[pos] == {'call': 'string_guard', 'index': i,
                                          'offset': offset, 'null': False, 'empty': empty}
                    pos += 1
                    counts['empty_guards' if empty else 'nonempty_guards'] += 1
                    if empty:
                        break
                    end = raw.index(0, offset)
                    value = raw[offset:end].decode('latin1')
                    if row['debug'] > 1:
                        assert events[pos] == {'call': 'debug', 'format': "cid: STR: '%s'\n",
                                               'text': value, 'offset': offset, 'result': 0}
                        pos += 1
                    length = {'call': 'strlen', 'offset': offset, 'text': value, 'result': len(value)}
                    assert events[pos] == length
                    assert events[pos + 1] == {'call': 'tty', 'length': len(value),
                                               'payload_hex': raw[offset:end].hex(), 'offset': offset, 'result': len(value)}
                    assert events[pos + 2] == {'call': 'tty', 'length': 2, 'payload_hex': '0d0a', 'offset': None, 'result': 2}
                    assert events[pos + 3] == length
                    pos += 4
                    offset = end + 1
                    delivered += 1
                delivered_fields[str(delivered)] += 1
                if delivered == 4:
                    counts['four_nonempty_calls'] += 1
                    if raw[offset]:
                        counts['four_field_cap_with_remaining'] += 1
            if used + p['n_in'] < row['count']:
                counts['early_settled_calls'] += 1
            break
        assert pos == len(events) and row['result'] == result
        counts['wrapper_result_' + str(result)] += 1
        counts.update('event_' + e['call'] for e in events)
    assert set(progress_results) == {'0', '1', '2'}
    assert {0, 2} == {row['debug'] for row in sides['original']}
    assert counts['early_settled_calls'] > 0
    assert counts['empty_guards'] > 0 and counts['nonempty_guards'] > 0
    assert resamplers['8000'] > 0, 'no observed 8000-Hz resampler calls'
    if require_cap:
        assert counts['four_nonempty_calls'] > 0, 'no observed four nonempty string iterations'
        assert counts['four_field_cap_with_remaining'] > 0, 'no observed fifth field withheld at the four-field cap'
    if require_raw9600:
        required = {f'{rate}/{value}/{debug}' for rate in (8000, 9600) for value in (1, 2) for debug in (0, 2)}
        assert required <= set(success_shapes), 'missing rate/raw/debug success coverage'
    return {'per_side': dict(counts), 'progress_results_per_side': dict(progress_results),
            'available_fields_per_success': dict(available_fields),
            'delivered_fields_per_success': dict(delivered_fields),
            'settled_packets_per_side': dict(packets),
            'success_rate_value_debug_per_side': dict(success_shapes),
            'constructor_retunes_per_side': dict(retunes),
            'native_resampler_calls_per_side': dict(resamplers),
            'progress_input_counts_per_side': dict(chunks),
            'create_calls_both_sides': len(trace['lifecycles']),
            'accepted_created_and_deleted_both_sides': sum(row['created'] for row in trace['lifecycles']),
            'paired_process_calls': len(sides['original']),
            'ordered_events_both_sides': sum(len(r['events']) for r in records)}


def controls(trace, require_cap=False, require_raw9600=False):
    caught = []
    mutations = ('progress-result', 'count-out', 'string-offset', 'callback-order')
    for mutation, both in [(m, False) for m in mutations] + [('count-out', True), ('callback-order', True), ('no-observations', False)]:
        bad = copy.deepcopy(trace)
        if mutation == 'no-observations':
            bad['records'] = []
        else:
            for side in ('original', 'candidate') if both else ('candidate',):
                row = next(r for r in bad['records'] if r['kind'] == side and r['result'] == 1)
                if mutation in ('progress-result', 'count-out'):
                    event = next(e for e in row['events'] if e['call'] == 'progress')
                    event['result' if mutation == 'progress-result' else 'n_out'] ^= 1
                elif mutation == 'string-offset':
                    next(e for e in row['events'] if e['call'] == 'string_guard')['offset'] += 1
                else:
                    index = next(i for i, e in enumerate(row['events']) if e['call'] == 'tty')
                    row['events'][index], row['events'][index + 1] = row['events'][index + 1], row['events'][index]
        try:
            validate(bad, require_cap, require_raw9600)
        except AssertionError:
            caught.append(mutation + ('-both-sides' if both else ''))
        else:
            raise AssertionError('negative control accepted: ' + mutation)
    if require_cap:
        for mutation in ('fourth-guard-index-both-sides', 'fifth-delivery-both-sides',
                         'packet-drift-preserved-checksum-both-sides'):
            bad = copy.deepcopy(trace)
            for side in ('original', 'candidate'):
                row = next(r for r in bad['records'] if r['kind'] == side
                           and any(e['call'] == 'get_strings' and
                                   len([field for field in bytes.fromhex(e['strings_hex']).split(b'\0') if field]) > 4
                                   for e in r['events']))
                guard = next(e for e in row['events'] if e['call'] == 'string_guard' and e['index'] == 3)
                if mutation.startswith('packet'):
                    event = next(e for e in row['events'] if e['call'] == 'progress' and e['result'] == 1)
                    packet = bytearray.fromhex(event['packet_hex'])
                    packet[-2] ^= 1
                    packet[-1] = -sum(packet[:-1]) & 255
                    assert sum(packet) % 256 == 0
                    event['packet_hex'] = packet.hex()
                elif mutation.startswith('fourth'):
                    guard['index'] = 4
                else:
                    # Model a full fifth iteration from the actual rendered
                    # extra field, on both sides: equality must not excuse it.
                    raw = bytes.fromhex(next(e for e in row['events'] if e['call'] == 'get_strings')['strings_hex'])
                    fields = raw.split(b'\0')
                    offset = sum(len(field) + 1 for field in fields[:4])
                    value = fields[4].decode('latin1')
                    row['events'].append({'call': 'string_guard', 'index': 4,
                                          'offset': offset, 'null': False, 'empty': False})
                    if row['debug'] > 1:
                        row['events'].append({'call': 'debug', 'format': "cid: STR: '%s'\n",
                                              'text': value, 'offset': offset, 'result': 0})
                    length = {'call': 'strlen', 'offset': offset, 'text': value, 'result': len(value)}
                    row['events'].extend([length,
                        {'call': 'tty', 'offset': offset, 'payload_hex': fields[4].hex(),
                         'length': len(value), 'result': len(value)},
                        {'call': 'tty', 'offset': None, 'payload_hex': '0d0a', 'length': 2, 'result': 2},
                        copy.deepcopy(length)])
            try:
                validate(bad, require_cap, require_raw9600)
            except AssertionError:
                caught.append(mutation)
            else:
                raise AssertionError('negative control accepted: ' + mutation)
    if require_raw9600:
        for mutation in ('raw-includes-checksum-both-sides', '9600-resample-both-sides',
                         'wrong-oscillator-rate-both-sides', 'changed-wave-sample-both-sides',
                         'raw-uppercase-both-sides', 'raw-byte-order-both-sides',
                         'missing-retune-both-sides', 'missing-raw9600-coverage'):
            bad = copy.deepcopy(trace)
            if mutation.startswith('missing-retune'):
                for row in bad['lifecycles']:
                    if row['created'] and row['rate'] == 9600:
                        row['retunes'] = []
            elif mutation.endswith('coverage'):
                bad['records'] = [r for r in bad['records'] if (r['rate'], r['cid_val']) != (9600, 2)]
            else:
                for side in SITES:
                    row = next(r for r in bad['records'] if r['kind'] == side
                               and r['rate'] == 9600 and r['cid_val'] == 2 and r['result'] == 1)
                    if mutation.startswith('wrong-oscillator'):
                        wrong = reference_wave(row['wire_hex'], row['rate'], 8000)
                        row['wave_sha256'] = hashlib.sha256(wrong).hexdigest()
                    elif mutation.startswith('changed-wave'):
                        wrong = bytearray(reference_wave(row['wire_hex'], row['rate']))
                        wrong[0] ^= 1
                        row['wave_sha256'] = hashlib.sha256(wrong).hexdigest()
                    elif mutation.startswith('9600'):
                        next(e for e in row['events'] if e['call'] == 'progress')['resampler_counts'] = [192]
                    else:
                        packet = bytes.fromhex(next(e for e in row['events'] if e['call'] == 'progress' and e['result'] == 1)['packet_hex'])
                        if mutation.startswith('raw-uppercase'):
                            wrong = packet[:-1].hex().upper().encode()
                            assert wrong != packet[:-1].hex().encode()
                        elif mutation.startswith('raw-byte-order'):
                            wrong = packet[:-1][::-1].hex().encode()
                        else:
                            wrong = packet.hex().encode()
                        next(e for e in row['events'] if e['call'] == 'get_strings')['strings_hex'] = (wrong + bytes(600 - len(wrong))).hex()
            try:
                validate(bad, require_cap, require_raw9600)
            except AssertionError:
                caught.append(mutation)
            else:
                raise AssertionError('negative control accepted: ' + mutation)
    return caught


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture', nargs='?', type=Path, default=ROOT / 'build/period/t_cid')
    parser.add_argument('--period-object', type=Path)
    parser.add_argument('--reference-object', type=Path, default=Path(d.b.BLOB))
    parser.add_argument('--out', type=Path, default=ROOT / 'build/cid-process-trace')
    parser.add_argument('--require-four-field-cap', action='store_true',
                        help='require four nonempty fields and a rendered fifth field withheld from TTY')
    parser.add_argument('--require-raw9600', action='store_true',
                        help='require successful raw/formatted delivery at 8000/9600 Hz and debug 0/2')
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    objpath = (args.period_object or fixture.parent / 'src_service_cid.o').resolve()
    refpath = args.reference_object.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    before = {str(p): sha(p) for p in (fixture, objpath, refpath)}
    snapshot = out / 't_cid-period'
    assert snapshot != fixture
    shutil.copy2(fixture, snapshot)
    assert sha(snapshot) == before[str(fixture)]
    linked = Object(snapshot)
    assert linked.elf['e_type'] == 'ET_EXEC' and linked.elf.get_section_by_name('.interp') is None
    assert GENTOO in linked.comments and set(linked.comments) <= {GENTOO, RUNTIME}
    _, carrier_bytes = linked.body('sintab')
    assert len(carrier_bytes) == 128
    carrier = [int.from_bytes(carrier_bytes[i:i + 2], 'little', signed=True)
               for i in range(0, 128, 2)]
    carrier_proof = check_carrier(carrier)
    bad_carrier = carrier[:]
    bad_carrier[8] += 2
    try:
        check_carrier(bad_carrier)
    except AssertionError:
        pass
    else:
        raise AssertionError('changed carrier sample accepted')
    settings, evidence = {}, {}
    for kind, path in [('original', refpath), ('candidate', objpath)]:
        obj = Object(path)
        assert obj.elf['e_type'] == 'ET_REL' and obj.comments == [GENTOO]
        prefix = 'ref_' if kind == 'original' else ''
        proof = verify_body(obj, linked, 'CID_process', prefix + 'CID_process', kind)
        size, loop, sites = SITES[kind]
        assert proof['bytes'] == size
        assert proof['normalized_sha256'] == BODY_HASHES[kind], 'unreviewed CID_process body'
        _, code = linked.body(prefix + 'CID_process')
        for offset, name in sites:
            assert code[offset] == 0xe8
            rel = int.from_bytes(code[offset + 1:offset + 5], 'little', signed=True)
            target = (proof['address'] + offset + 5 + rel) & 0xffffffff
            targetname = CALL_NAMES[name]
            if kind == 'original' and 'ref_' + targetname in linked.symbols:
                targetname = 'ref_' + targetname
            if name == 'tty':
                targetname = '__wrap_' + prefix + 'modem_send_to_tty'
            assert target == linked.symbols[targetname]['st_value']
        settings[kind] = {name: linked.symbols[prefix + 'CID_' + name]['st_value']
                          for name in ('create', 'delete', 'process')}
        settings[kind].update(debug=linked.symbols[prefix + 'dsplibs_debug_level']['st_value'],
                              sites=sites, loop=loop)
        for name, size in [('bits', 4096), ('nbits', 4), ('line_rate', 4), ('nsamp', 4), ('burst', 80000)]:
            symbols = [s for s in linked.symtab.iter_symbols() if s.name == name and s['st_size'] == size]
            assert len(symbols) == 1, (name, 'ambiguous fixture wire owner')
            settings[kind][name] = symbols[0]['st_value']
        settings[kind]['carrier'] = linked.symbols['sintab']['st_value']
        settings[kind]['retune'] = linked.symbols[prefix + 'cid_freq_sampl']['st_value']
        settings[kind]['resampler'] = linked.symbols[prefix + 'FPM_MRF_filter']['st_value']
        modem = linked.symbols[prefix + 'cid_modem']
        settings[kind]['modem_range'] = [modem['st_value'], modem['st_value'] + modem['st_size']]
        evidence[kind] = dict(proof, object=str(path), object_sha256=before[str(path)], comments=obj.comments)
    # Preserve original companion exacts; this apparatus never changes source.
    companions = {name: d.b.verdict(*d.b.body(str(refpath), name), *d.b.body(str(objpath), name))
                  for name in ('CID_create', 'CID_delete')}
    assert all(v[0] == 'EXACT' for v in companions.values()), companions
    (out / 'trace-settings.json').write_text(json.dumps(settings, indent=2) + '\n')
    command = ['gdb', '-q', '-batch', '-ex', 'set pagination off', '-ex',
               'source ' + str(ROOT / 'tools/cid_process_trace_gdb.py'), '--args', str(snapshot)]
    with (out / 'gdb.log').open('w') as log:
        subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
    trace = json.loads((out / 'trace-observe.json').read_text())
    report = validate(trace, args.require_four_field_cap, args.require_raw9600)
    report.update(fixture=str(fixture), fixture_sha256=before[str(fixture)], evidence=evidence,
                  input_hashes=before, command=command, companions=companions,
                  negative_controls=controls(trace, args.require_four_field_cap, args.require_raw9600),
                  four_field_cap_required=args.require_four_field_cap, production_source_changed=False)
    report['raw9600_required'] = args.require_raw9600
    report['carrier_calibration'] = carrier_proof
    report['negative_controls'].append('changed-period-carrier-sample')
    assert before == {p: sha(Path(p)) for p in before}, 'input changed during observation'
    assert sha(snapshot) == before[str(fixture)], 'snapshot changed'
    report['fixture_and_objects_unchanged'] = True
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    negatives = len(report['negative_controls'])
    print(f"{report['paired_process_calls']} paired CID_process calls / {report['ordered_events_both_sides']} ordered events agree; {negatives}/{negatives} detector controls rejected")


if __name__ == '__main__':
    main()
