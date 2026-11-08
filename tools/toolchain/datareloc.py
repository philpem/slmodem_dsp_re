#!/usr/bin/env python3
"""Audit ELF32 data initializers by relocation targets, not packed addresses.

This is a discrepancy inventory, not an equivalence or byte-identity gate.
LOCAL names retain FILE ownership. GLOBAL names imply no defining-TU claim.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import elfinfo
import partialcmp


class Unresolved(ValueError):
    pass


class Model:
    def __init__(self, path):
        self.path = Path(path)
        partialcmp.validate_elf(path)
        self.raw = self.path.read_bytes()
        self.sections = {s['index']: s for s in partialcmp.sections(path, True)}
        if sum(s['type'] in ('SYMTAB', 'DYNSYM') for s in self.sections.values()) != 1:
            raise ValueError('exactly one symbol table is required')
        self.symbols = elfinfo.read_symbols(path)
        self.bynum = {s.num: s for s in self.symbols}
        self.owners = {}
        self.file_counts = Counter(s.name for s in self.symbols if s.type == 'FILE')
        owner = None
        for symbol in self.symbols:
            if symbol.type == 'FILE':
                owner = symbol.name
            elif symbol.bind == 'LOCAL':
                self.owners[symbol.num] = owner
        self.objects = defaultdict(list)
        for symbol in self.symbols:
            if symbol.type == 'OBJECT' and symbol.ndx.isdigit():
                sec = self.sections[int(symbol.ndx)]
                if 'A' in sec['flags'] and symbol.size:
                    self.objects[self.key(symbol)].append(symbol)
        self.relocations = defaultdict(list)
        for sec in self.sections.values():
            if sec['type'] == 'RELA':
                raise ValueError('RELA is outside the ELF32 REL audit domain')
            if sec['type'] != 'REL':
                continue
            if sec['entsize'] != 8 or sec['size'] % 8:
                raise ValueError('invalid REL extent')
            for offset in range(sec['offset'], sec['offset'] + sec['size'], 8):
                at, info = struct.unpack_from('<II', self.raw, offset)
                self.relocations[sec['info']].append((at, info & 255, info >> 8))

    def key(self, symbol):
        return (symbol.name, symbol.bind,
                self.owners.get(symbol.num) if symbol.bind == 'LOCAL' else None)

    def identity(self, symbol):
        key = self.key(symbol)
        if symbol.bind == 'LOCAL' and (not key[2] or self.file_counts[key[2]] != 1):
            raise Unresolved('missing-or-repeated-LOCAL-FILE-owner')
        return (symbol.type,) + key

    def payload(self, symbol):
        sec = self.sections[int(symbol.ndx)]
        if symbol.value + symbol.size > sec['size']:
            raise Unresolved('object-outside-section')
        if sec['type'] == 'NOBITS':
            return bytes(symbol.size)
        start = sec['offset'] + symbol.value
        if start + symbol.size > len(self.raw):
            raise ValueError('object payload extends beyond EOF')
        return self.raw[start:start + symbol.size]

    def resolve(self, number, addend):
        target = self.bynum[number]
        if not target.ndx.isdigit():
            raise Unresolved('undefined-absolute-or-COMMON-target')
        sec = self.sections[int(target.ndx)]
        if 'G' in sec['flags'] or sec['name'].startswith('.gnu.linkonce.'):
            raise Unresolved('COMDAT-or-linkonce-target')
        address = (target.value + addend) & 0xffffffff
        if target.type in ('OBJECT', 'FUNC'):
            if not target.value <= address < target.value + target.size:
                raise Unresolved('named-target-addend-outside-extent')
            return ('symbol', self.identity(target), address - target.value)
        if target.type != 'SECTION':
            raise Unresolved('unsupported-target-kind')
        containers = [s for s in self.symbols if s.type in ('OBJECT', 'FUNC')
                      and s.ndx == target.ndx and s.value <= address < s.value + s.size]
        if len(containers) > 1:
            raise Unresolved('overlapping-target-symbols')
        if containers:
            symbol = containers[0]
            return ('symbol', self.identity(symbol), address - symbol.value)
        # Only linker-mergeable byte strings may use this content descriptor.
        # String equality does not establish address identity or aliasing.
        if 'S' in sec['flags'] and 'M' in sec['flags'] and sec['entsize'] == 1:
            data = self.raw[sec['offset']:sec['offset'] + sec['size']]
            if address >= len(data) or (address and data[address - 1] != 0):
                raise Unresolved('interior-or-outside-string-address')
            end = data.find(b'\0', address)
            if end < 0:
                raise Unresolved('unterminated-string')
            return ('mergeable-string-content', data[address:end + 1].hex())
        raise Unresolved('uncovered-section-address')

    def initializer(self, symbol):
        self.identity(symbol)
        sec = self.sections[int(symbol.ndx)]
        if 'G' in sec['flags'] or sec['name'].startswith('.gnu.linkonce.'):
            raise Unresolved('COMDAT-or-linkonce-source')
        payload = bytearray(self.payload(symbol))
        pointers = []
        used = set()
        for at, kind, number in sorted(self.relocations[int(symbol.ndx)]):
            # A four-byte REL field starting up to three bytes before the
            # object overlaps its first byte and must not be silently ignored.
            if not symbol.value - 3 <= at < symbol.value + symbol.size:
                continue
            offset = at - symbol.value
            if kind != 1:
                raise Unresolved('non-R_386_32-relocation')
            if offset < 0 or offset + 4 > symbol.size:
                raise Unresolved('relocation-crosses-object-boundary')
            fields = set(range(offset, offset + 4))
            if fields & used:
                raise Unresolved('overlapping-relocation-fields')
            used |= fields
            addend = struct.unpack_from('<I', payload, offset)[0]
            pointers.append((offset, self.resolve(number, addend)))
            payload[offset:offset + 4] = b'\0' * 4
        return ((sec['name'], sec['flags'], symbol.size), payload.hex(), pointers)

    def relocated(self, symbol):
        return any(symbol.value - 3 <= at < symbol.value + symbol.size
                   for at, _, _ in self.relocations[int(symbol.ndx)])


def compare(left, right):
    rows = []
    for key, objects in sorted(left.objects.items(), key=lambda item: str(item[0])):
        others = right.objects.get(key, [])
        row = {'key': key, 'relocated': any(left.relocated(s) for s in objects)
               or any(right.relocated(s) for s in others)}
        if len(objects) != 1 or len(others) > 1:
            row.update(status='ambiguous', reason='nonunique-object-key')
        elif not others:
            row.update(status='different', reason='missing-object-key')
        else:
            try:
                a, b = left.initializer(objects[0]), right.initializer(others[0])
                row.update(status='matched' if a == b else 'different',
                           reference=a, candidate=b)
            except Unresolved as error:
                row.update(status='unsupported', reason=str(error))
        rows.append(row)
    return {
        'reference': str(left.path), 'candidate': str(right.path),
        'reference_sha256': hashlib.sha256(left.raw).hexdigest(),
        'candidate_sha256': hashlib.sha256(right.raw).hexdigest(),
        'reference_keys': len(left.objects), 'candidate_keys': len(right.objects),
        'counts': dict(Counter(r['status'] for r in rows)),
        'relocated_counts': dict(Counter(r['status'] for r in rows if r['relocated'])),
        'extra_candidate_keys': sorted(set(right.objects) - set(left.objects), key=str),
        'rows': rows,
        'limits': ['Initializer descriptors only; no function-body equivalence.',
                   'Mergeable strings compare contents, not address identity or aliasing.',
                   'No GLOBAL defining-TU inference; no post-link localization model.',
                   'Unsupported and ambiguous records are not matches.'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--json', required=True, type=Path)
    args = parser.parse_args()
    import json
    report = compare(Model(args.reference), Model(args.candidate))
    args.json.write_text(json.dumps(report, indent=2) + '\n')
    print('data initializer audit:', report['reference_keys'], 'reference keys,',
          report['candidate_keys'], 'candidate keys;', report['counts'])
    print('relocated subset:', sum(report['relocated_counts'].values()),
          report['relocated_counts'], '; candidate extras:', len(report['extra_candidate_keys']))
    if not report['reference_keys']:
        raise SystemExit('zero reference denominator')


if __name__ == '__main__':
    main()
