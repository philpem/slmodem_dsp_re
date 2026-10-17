#!/usr/bin/env python3
"""Trace ordered CID_process boundaries in an existing static Gentoo fixture.

This observes public-create/feed/delete component histories, not modem end-to-end
reachability. Only calls made directly by CID_process are expanded; receiver
internals remain inside cid_progress. No compilation, input mutation, inferior
calls or production source changes are performed. Debug/TTY return values are
the harness host contract; CID_process itself ignores those int results.
"""
import playbook_small_patterns as d
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from collections import Counter
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


def validate(trace):
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
    counts = Counter()
    progress_results = Counter()
    chunks = Counter()
    for row in sides['original']:
        counts['process_calls'] += 1
        counts['debug_level_' + str(row['debug'])] += 1
        events = row['events']
        pos, used, result = 0, 0, 0
        while used < row['count']:
            p = events[pos]
            assert p['call'] == 'progress'
            assert p['input_offset'] == used and p['n_in'] == min(192, row['count'] - used)
            assert p['what'] == 0 and p['n_out'] == 0
            assert p['state_in'][:2] == [0, 1] and p['state_out'][:2] == [0, 1]
            progress_results[str(p['result'])] += 1
            chunks[str(p['n_in'])] += 1
            pos += 1
            if not p['result']:
                used += p['n_in']
                continue
            result = 1 if p['result'] == 1 else -1
            if row['debug'] > 1:
                msg = 'cid: CID Success\n' if result == 1 else 'cid: CID Failure\n'
                assert events[pos] == {'call': 'debug', 'format': msg, 'result': 0}
                pos += 1
            if result == 1:
                strings = events[pos]
                assert strings['call'] == 'get_strings' and strings['result_offset'] == 8
                raw = bytes.fromhex(strings['strings_hex'])
                pos += 1
                offset = 0
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
    return {'per_side': dict(counts), 'progress_results_per_side': dict(progress_results),
            'progress_input_counts_per_side': dict(chunks),
            'create_calls_both_sides': len(trace['lifecycles']),
            'accepted_created_and_deleted_both_sides': sum(row['created'] for row in trace['lifecycles']),
            'paired_process_calls': len(sides['original']),
            'ordered_events_both_sides': sum(len(r['events']) for r in records)}


def controls(trace):
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
            validate(bad)
        except AssertionError:
            caught.append(mutation + ('-both-sides' if both else ''))
        else:
            raise AssertionError('negative control accepted: ' + mutation)
    return caught


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture', nargs='?', type=Path, default=ROOT / 'build/period/t_cid')
    parser.add_argument('--period-object', type=Path)
    parser.add_argument('--reference-object', type=Path, default=Path(d.b.BLOB))
    parser.add_argument('--out', type=Path, default=ROOT / 'build/cid-process-trace')
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
    report = validate(trace)
    report.update(fixture=str(fixture), fixture_sha256=before[str(fixture)], evidence=evidence,
                  input_hashes=before, command=command, companions=companions,
                  negative_controls=controls(trace), production_source_changed=False)
    assert before == {p: sha(Path(p)) for p in before}, 'input changed during observation'
    assert sha(snapshot) == before[str(fixture)], 'snapshot changed'
    report['fixture_and_objects_unchanged'] = True
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    negatives = len(report['negative_controls'])
    print(f"{report['paired_process_calls']} paired CID_process calls / {report['ordered_events_both_sides']} ordered events agree; {negatives}/{negatives} changed-trace controls rejected")


if __name__ == '__main__':
    main()
