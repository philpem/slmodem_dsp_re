#!/usr/bin/env python3
"""Resolve the three diagnostic census leads to original string consumers.

Counts nominate review targets. Shared tails, duplicated arms and inlined
callees are modelled explicitly here; they are not source omissions.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

_saved = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
from elftools.elf.elffile import ELFFile
sys.path[:] = _saved
ROOT = Path(__file__).resolve().parents[1]
ROWS = [('B103prc', 'B103AnswerNextState'), ('Rxcid', 'create_cid'), ('V8', 'evaluateRxJMSequence')]


def strings(path, name):
    refs = []
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        owner = next(s for s in table.iter_symbols() if s.name == name)
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL' or section['sh_info'] != owner['st_shndx']:
                continue
            code = elf.get_section(section['sh_info']).data()
            names = elf.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                off = relocation['r_offset']
                if not owner['st_value'] <= off < owner['st_value'] + owner['st_size']:
                    continue
                target = names.get_symbol(relocation['r_info_sym'])
                if not isinstance(target['st_shndx'], int):
                    continue
                data = elf.get_section(target['st_shndx'])
                if not data.name.startswith('.rodata.str'):
                    continue
                addend = int.from_bytes(code[off:off+4], 'little', signed=True)
                begin = target['st_value'] + addend
                raw = data.data()
                end = raw.index(b'\0', begin)
                refs.append({'at': hex(off), 'text': raw[begin:end].decode('ascii')})
    return refs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=ROOT/'build/diagnostic-audit-next')
    ap.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    args = ap.parse_args()
    matrix = json.loads((args.run_dir/'results.json').read_text())['families']
    report = {'reference_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(), 'functions': {}}
    for family, name in ROWS:
        cell = matrix[family]['cells']['baseline']
        assert cell['baseline_reproduced'], family
        path = args.run_dir/family/'baseline/candidate.o'
        before, after = strings(args.reference, name), strings(path, name)
        expected = Counter(r['text'] for r in before)
        actual = Counter(r['text'] for r in after)
        if family == 'Rxcid':
            expected.update(r['text'] for r in strings(args.reference, 'reset_cid'))
        elif family == 'V8':
            expected['V8: call function DATA indication...\r\n'] += 1
        assert actual == expected, (family, actual, expected)
        wrong = actual.copy()
        first = next(iter(wrong))
        wrong[first] -= 1
        wrong[first + ' WRONG'] += 1
        assert wrong != expected, 'format negative did not fire'
        report['functions'][name] = {'original_string_consumers': before, 'candidate_string_consumers': after,
            'explained_string_refs': sum(actual.values()), 'format_negative_rejected': True,
            'complete_TU_bodies': len(cell['functions']),
            'exact_bodies': sum(v[0] == 'EXACT' for v in cell['verdicts'].values()),
            'raw_baseline_sha256': cell['object_hash']}
    report['mechanisms'] = {
        'B103AnswerNextState': 'Original default ordinary call plus START/CARRDET/WAIT1 shared sibling tail. Candidate splits the sibling tail; four guarded formats and all state/flag/status stores agree.',
        'create_cid': 'Candidate inlines reset_cid and its Reset diagnostic. Original calls reset_cid on both Creating guard paths. History null precedes reset; rate8000/threshold2/step9 follow; final args are unsigned rate reload and literal2.',
        'evaluateRxJMSequence': 'Original DATA0x107 and V800x109 route to shared DATA print76c07; candidate has two print sites. All other format consumers agree. Original mask0xfff1, function words107/103/10b/109, capability bits40/01/80/02, extension matching and final fn_matched-only verdict agree with source.'}
    report['inference_boundary'] = 'String relocation consumers are not a path-equivalence proof. No missing behavior demonstrated in the selected guard, argument and publication boundaries; no source edit justified by counts.'
    report['next_discriminator'] = 'Independent valid lifecycle transcript disagreement or ordinary helper/value-age discrepancy; do not force CFG tail sharing or suppress inlining to equalise counts.'
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Three raw TU baselines;3/3 resolved-format inventories explained;3/3 wrong-format negatives rejected')


if __name__ == '__main__':
    main()
