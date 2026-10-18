#!/usr/bin/env python3
"""Replay the native signed-word V34 snapshot index and audit every emitted body."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
_paths = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT / 'tools']
import dis
sys.path[:] = _paths
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
from recovery_b103_helper_boundary import pair_metadata, json_value
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b
from elftools.elf.elffile import ELFFile

REV = 'dfc23b7d1'
HEADER = 'include/dsplib/v34hstx1_arms.h'
SOURCE = 'src/pump/v34/V34hshak.c'
FIXTURE = 'test/unit/t_v34hstx1.c'


def pinned(path):
    return subprocess.check_output(['git', 'show', REV + ':' + path], cwd=ROOT, text=True)


def symbols(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        return {s.name: (s['st_value'], s['st_size'])
                for s in elf.get_section_by_name('.symtab').iter_symbols()
                if s['st_info']['type'] == 'STT_FUNC' and isinstance(s['st_shndx'], int)}


def canonical_nontext(path):
    """Resolve text-section addends to actual local/global function ownership."""
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        functions = [s for s in table.iter_symbols()
                     if s['st_info']['type'] == 'STT_FUNC' and isinstance(s['st_shndx'], int)]
        rows, masked = [], {}
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            target = elf.get_section(section['sh_info'])
            if target.name == '.text':
                continue
            data = bytearray(target.data())
            for relocation in section.iter_relocations():
                symbol = table.get_symbol(relocation['r_info_sym'])
                offset = relocation['r_offset']
                addend = int.from_bytes(data[offset:offset + 4], 'little')
                destination = [symbol.name, addend]
                if (isinstance(symbol['st_shndx'], int)
                        and symbol['st_info']['type'] == 'STT_SECTION'
                        and elf.get_section(symbol['st_shndx']).name == '.text'):
                    owners = [s for s in functions if s['st_shndx'] == symbol['st_shndx']
                              and s['st_value'] <= addend < s['st_value'] + s['st_size']]
                    assert len(owners) == 1, (hex(addend), owners)
                    destination = [owners[0].name, addend - owners[0]['st_value']]
                rows.append([target.name, offset, relocation['r_info_type'], destination])
                data[offset:offset + 4] = b'\0' * 4
            masked[target.name] = data.hex()
        return {'relocations': rows, 'masked_sections': masked}


def dispatcher_call(path):
    values = symbols(path)
    raw = b.body(str(path), 'v34handshak')[0]
    offset = 0x91a
    assert raw[offset] == 0xe8
    destination = values['v34handshak'][0] + offset + 5 + int.from_bytes(
        raw[offset + 1:offset + 5], 'little', signed=True)
    assert destination == values['v34tx1_trnseg4a'][0]
    normalized = raw[:offset + 1] + b'\0' * 4 + raw[offset + 5:]
    return normalized, {'offset': offset, 'destination': 'v34tx1_trnseg4a'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=ROOT / 'build/snapshot-dfc-baseline')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    header = pinned(HEADER)
    before = '\tint sum = 0, i, t;'
    assert header.count(before) == 1
    candidate = header.replace(before, '\tint sum = 0, t;\n\tshort i;')
    assert candidate == (ROOT / HEADER).read_text()
    # The target header is included directly by one production TU and one fixture;
    # no header includes it, so there is no hidden transitive consumer.
    consumers = []
    for directory in ('src', 'include', 'test'):
        for path in (ROOT / directory).rglob('*'):
            if path.suffix not in ('.c', '.cpp', '.h'):
                continue
            if re.search(r'^\s*#\s*include\s+["<]dsplib/v34hstx1_arms\.h[">]', path.read_text(), re.M):
                consumers.append(str(path.relative_to(ROOT)))
    assert sorted(consumers) == sorted([SOURCE, FIXTURE]), consumers
    config = (args.objects / '.build-config').read_text()
    image = next(line[6:] for line in config.splitlines() if line.startswith('image '))
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/tools/toolchain/period_compat.h'
             if f == 'tools/toolchain/period_compat.h' else f for f in flags] + ['-da']
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    report = {'revision': REV, 'config': config, 'header_consumers': consumers,
              'source_hashes': {p: hashlib.sha256(pinned(p).encode()).hexdigest()
                                for p in (SOURCE, FIXTURE)}, 'cells': {}}
    objects = {}
    for label, text in [('baseline', header), ('index-word', candidate)]:
        directory = out / label
        (directory / 'include/dsplib').mkdir(parents=True, exist_ok=True)
        (directory / 'include/dsplib/v34hstx1_arms.h').write_text(text)
        commands = []
        for source, fixture in [(SOURCE, False), (FIXTURE, True)]:
            name = Path(source).name
            local_source = Path(source) if fixture else Path(name)
            (directory / local_source).parent.mkdir(parents=True, exist_ok=True)
            (directory / local_source).write_text(pinned(source))
            target = '/work/' + label
            cell_flags = ['-I' + target + '/include'] + flags
            if fixture:
                cell_flags += ['-I/src/test/harness', '-std=gnu99']
            command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                    tc.GENTOO_COMPILER_PATH, cell_flags, target + '/' + name + '.o', source if fixture else target + '/' + name)]
            with (directory / (name + '.compile.log')).open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
            commands.append(command)
        obj = directory / 'V34hshak.c.o'
        objects[label] = obj
        if label == 'baseline':
            assert obj.read_bytes() == (args.objects / 'src_pump_v34_V34hshak.c.o').read_bytes()
        names = b.sizes(str(obj))
        common = set(names) & set(b.sizes(b.BLOB))
        assert len(names) == 53 and len(common) == 29
        rows = {name: {'bytes': len(b.body(str(obj), name)[0]),
                      'body_sha256': hashlib.sha256(b.body(str(obj), name)[0]).hexdigest(),
                      'relocations': b.body(str(obj), name)[1]}
                for name in names}
        for name in common:
            rows[name]['verdict'] = b.verdict(*b.body(b.BLOB, name), *b.body(str(obj), name))
        report['cells'][label] = {'commands': commands, 'all_emitted': 53, 'native_common': 29,
                                 'rows': rows, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                                 'header_sha256': hashlib.sha256(text.encode()).hexdigest(),
                                 'fixture_object_sha256': hashlib.sha256(
                                     (directory / 't_v34hstx1.c.o').read_bytes()).hexdigest()}
    base, selected = objects['baseline'], objects['index-word']
    assert b.sizes(str(base)).keys() == b.sizes(str(selected)).keys()
    changes = [name for name in b.sizes(str(base))
               if b.body(str(base), name) != b.body(str(selected), name)]
    assert set(changes) == {'v34tx1_trnseg4a', 'v34handshak'}
    left, proof = dispatcher_call(base)
    right, _ = dispatcher_call(selected)
    assert left == right
    old_meta = pair_metadata({'V34hshak': base})
    new_meta = pair_metadata({'V34hshak': selected})
    assert old_meta['definitions'] == new_meta['definitions']
    assert old_meta['external_imports'] == new_meta['external_imports']
    assert old_meta['sections'].keys() == new_meta['sections'].keys()
    for section in old_meta['sections']:
        if section == 'V34hshak:.rodata':
            assert old_meta['sections'][section][:4] == new_meta['sections'][section][:4]
        else:
            assert old_meta['sections'][section] == new_meta['sections'][section]
    assert canonical_nontext(base) == canonical_nontext(selected)
    fixture_before = out / 'baseline/t_v34hstx1.c.o'
    fixture_after = out / 'index-word/t_v34hstx1.c.o'
    assert b.sizes(str(fixture_before)).keys() == b.sizes(str(fixture_after)).keys()
    report['fixture_body_audit'] = {
        'all_emitted': len(b.sizes(str(fixture_before))),
        'changed': [n for n in b.sizes(str(fixture_before))
                    if b.body(str(fixture_before), n) != b.body(str(fixture_after), n)]}
    old_rows = report['cells']['baseline']['rows']
    new_rows = report['cells']['index-word']['rows']
    gains = [n for n in old_rows if 'verdict' in old_rows[n]
             and old_rows[n]['verdict'][0] != 'EXACT' and new_rows[n]['verdict'][0] == 'EXACT']
    losses = [n for n in old_rows if 'verdict' in old_rows[n]
              and old_rows[n]['verdict'][0] == 'EXACT' and new_rows[n]['verdict'][0] != 'EXACT']
    assert not gains and not losses
    report.update(changed_bodies=changes, dispatcher_call_proof=proof, exact_gains=gains,
                  exact_losses=losses, canonical_nontext=canonical_nontext(selected),
                  metadata_before=old_meta, metadata_after=new_meta)
    (out / 'results.json').write_text(json.dumps(report, indent=2, default=json_value) + '\n')
    print('2 endpoints, 53 emitted bodies each, 29 native-common; no exact gains/losses')
    print('Only snapshot parent and proven same-target dispatcher call displacement change')
    print('All nontext destinations/interior offsets, nonrelocation bytes and bindings preserved')


if __name__ == '__main__':
    main()
