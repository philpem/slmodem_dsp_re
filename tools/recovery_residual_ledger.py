#!/usr/bin/env python3
"""Inventory every reference function and explain bounded screen exclusions.

The canonical byte comparator decides grades. Local call resolution establishes
only an unambiguous ELF function start in the caller's section, not a return
contract, a reachable path or an aliasing relationship.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies
from elftools.elf.elffile import ELFFile


RANK = {'EXACT': 0, 'UNRESOLVED': 1, 'REGALLOC': 2, 'RELOC': 3,
        'BYTES': 4, 'SIZE': 5, 'NODATA': 6}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def function_owners(path):
    by_name, by_start = defaultdict(set), defaultdict(set)
    with Path(path).open('rb') as stream:
        elf = ELFFile(stream)
        symtab = elf.get_section_by_name('.symtab')
        if symtab is None:
            raise ValueError('missing symbol table: %s' % path)
        for symbol in symtab.iter_symbols():
            section = symbol['st_shndx']
            if symbol['st_info']['type'] != 'STT_FUNC' or not isinstance(section, int):
                continue
            value, size = symbol['st_value'], symbol['st_size']
            by_name[symbol.name].add((section, value, size))
            if size:
                by_start[(section, value)].add(symbol.name)
    return by_name, by_start


def call_sites(path, parsed):
    owners, starts = function_owners(path)
    result = {}
    for name, rows in parsed.items():
        sites = []
        owner = owners.get(name, set())
        for row in rows:
            if row['mnemonic'] != 'call':
                continue
            site = {'address': row['address'], 'operands': row['operands']}
            if 'target' in row:
                site.update(kind='relocated', target=row['target'])
            elif row['operands'].startswith('*'):
                site.update(kind='indirect', refusal='callee identity not established')
            elif len(owner) != 1:
                site.update(kind='unresolved-direct', refusal='ambiguous caller section')
            else:
                target = re.match(r'^([0-9a-f]+)(?:\s|$)', row['operands'])
                names = starts.get((next(iter(owner))[0], int(target[1], 16)), set()) if target else set()
                if len(names) == 1:
                    site.update(kind='local-direct', target=next(iter(names)))
                else:
                    site.update(kind='unresolved-direct', refusal='not one function start in caller section')
            sites.append(site)
        result[name] = sites
    return result


def legacy_scope(source, size):
    path = Path(source)
    if 'v34' in path.parts or 'fax' in path.parts or path.name.startswith(('V34', 'vpcm')):
        return 'excluded-source-family'
    if size > 850:
        return 'above-850-original-bytes'
    if size <= 16:
        return 'at-most-16-original-bytes'
    return 'eligible'


def self_test():
    # No .globl declarations: the assembler resolves local direct calls
    # without a relocation. An indirect call must remain unknown.
    source = '''.text
.type caller,@function
caller: call local
call *%eax
ret
.size caller,.-caller
.type local,@function
local: ret
.size local,.-local
'''
    with tempfile.TemporaryDirectory(prefix='recovery-ledger-') as work:
        p = Path(work)
        (p/'base.s').write_text(source)
        subprocess.run(['as', '--32', '-o', str(p/'base.o'), str(p/'base.s')], check=True)
        parsed = bodies(p/'base.o')
        sites = call_sites(p/'base.o', parsed)['caller']
        assert sites[0]['kind'] == 'local-direct' and sites[0]['target'] == 'local'
        assert sites[1]['kind'] == 'indirect' and 'target' not in sites[1]
        (p/'alias.s').write_text(source + '.set alias,local\n.type alias,@function\n')
        subprocess.run(['as', '--32', '-o', str(p/'alias.o'), str(p/'alias.s')], check=True)
        ambiguous = call_sites(p/'alias.o', bodies(p/'alias.o'))['caller'][0]
        assert ambiguous['kind'] == 'unresolved-direct'
    assert legacy_scope('src/pump/v34/V34hshak.c', 61541) == 'excluded-source-family'
    assert legacy_scope('src/dsp/psd.cpp', 851) == 'above-850-original-bytes'
    assert legacy_scope('src/dsp/psd.cpp', 16) == 'at-most-16-original-bytes'
    assert legacy_scope('src/dsp/psd.cpp', 17) == 'eligible'
    print('ledger controls: 7 passed; local positive, indirect/alias refusals, 4 filter boundaries')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--objects', type=Path, default=d.ROOT/'build/tc_repro')
    ap.add_argument('--reference', type=Path, default=Path(d.b.BLOB))
    ap.add_argument('--census', type=Path)
    ap.add_argument('--baseline-manifest', type=Path,
                    help='archived source/object hashes from a validated build')
    ap.add_argument('--output', type=Path)
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return
    if args.census is None or args.output is None:
        ap.error('--census and --output are required')
    self_test()
    census = json.loads(args.census.read_text())
    input_manifest = json.loads(args.baseline_manifest.read_text()) if args.baseline_manifest else None
    config = (args.objects/'.build-config').read_text()
    if '-DDSPLIB_REPRODUCE_BUGS' not in config:
        raise ValueError('faithful build configuration required')
    manifest = []
    for line in (args.objects/'tc_manifest.txt').read_text().splitlines():
        object_name, source = line.split()
        manifest.append((object_name, source))
    actual = {p.name for p in args.objects.glob('*.o')}
    if actual != {name for name, source in manifest}:
        raise ValueError('object tree does not exactly match its manifest')
    source_hashes = {}
    if input_manifest:
        if (input_manifest['config'] != config or
                input_manifest['reference_sha256'] != sha(args.reference) or
                set(input_manifest['objects']) != actual):
            raise ValueError('archived manifest does not describe these build inputs')
        source_hashes = input_manifest['sources']
    ref_sizes = d.b.sizes(str(args.reference))
    ref_parsed = bodies(args.reference)
    ref_calls = call_sites(args.reference, ref_parsed)
    copies, hashes, extra = defaultdict(list), {}, []
    call_counts = Counter()
    for object_name, source in manifest:
        path = args.objects/object_name
        if input_manifest is None:
            if (d.ROOT/source).stat().st_mtime > path.stat().st_mtime:
                raise ValueError('source newer than object: %s' % source)
            source_hashes[source] = sha(d.ROOT/source)
        hashes[object_name] = sha(path)
        if input_manifest and hashes[object_name] != input_manifest['objects'][object_name]:
            raise ValueError('object disagrees with archived manifest: %s' % object_name)
        sizes = d.b.sizes(str(path))
        parsed = bodies(path)
        sites = call_sites(path, parsed)
        for name, size in sizes.items():
            if name not in ref_sizes:
                extra.append({'symbol': name, 'object': object_name, 'source': source, 'bytes': size})
                continue
            calls = sites.get(name, [])
            call_counts.update(site['kind'] for site in calls)
            copies[name].append({'object': object_name, 'source': source, 'bytes': size,
                                 'legacy_value_timing_scope': legacy_scope(source, ref_sizes[name]),
                                 'calls': calls})
    records, counts, filter_counts = [], Counter(), Counter()
    exact = set()
    for name in sorted(copies):
        original = d.b.body(str(args.reference), name)
        scores = []
        for copy in copies[name]:
            path = str(args.objects/copy['object'])
            raw = d.b.verdict(*original, *d.b.body(path, name))
            copy['raw_verdict'] = list(raw)
            scores.append((raw, copy))
        (grade, delta), worst = max(scores, key=lambda row: RANK[row[0][0]])
        if grade not in ('EXACT', 'UNRESOLVED', 'NODATA', 'RELOC'):
            if d.b.alpha_equal(d.b.insns(str(args.reference), name),
                               d.b.insns(str(args.objects/worst['object']), name)):
                grade, delta = 'REGALLOC', 0
        counts[grade] += 1
        if grade == 'EXACT':
            exact.add(name)
            scope = 'already-exact'
        else:
            scopes = {copy['legacy_value_timing_scope'] for copy in copies[name]}
            scope = 'eligible' if 'eligible' in scopes else '|'.join(sorted(scopes))
            filter_counts[scope] += 1
        records.append({'symbol': name, 'grade': grade, 'raw_delta': delta,
                        'original_bytes': ref_sizes[name], 'legacy_value_timing_scope': scope,
                        'original_calls': ref_calls.get(name, []), 'definitions': copies[name],
                        'compiler_stage_cause': 'not established'})
    expected = {key.upper(): census[key] for key in
                ('exact', 'regalloc', 'bytes', 'size', 'unresolved', 'reloc', 'nodata')}
    if any(counts[key] != value for key, value in expected.items()) or exact != set(census['exact_symbols']):
        raise ValueError('ledger regrading disagrees with canonical census: %s' % dict(counts))
    if any(sha(args.objects/name) != value for name, value in hashes.items()):
        raise ValueError('object inputs changed during ledger generation')
    if (args.objects/'.build-config').read_text() != config:
        raise ValueError('build configuration changed during ledger generation')
    if input_manifest is None and any(sha(d.ROOT/source) != value for source, value in source_hashes.items()):
        raise ValueError('source inputs changed during ledger generation')
    result = {'schema': 1, 'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=d.ROOT, text=True).strip(),
              'reference_sha256': sha(args.reference), 'census_sha256': sha(args.census),
              'build_config': config, 'object_sha256': hashes, 'object_count': len(manifest),
              'source_sha256': source_hashes,
              'source_identity_mode': 'archived-baseline' if input_manifest else 'current-source-stable',
              'baseline_manifest_sha256': sha(args.baseline_manifest) if input_manifest else None,
              'reference_function_count': len(ref_sizes), 'shared_unique': len(records),
              'shared_defining_copies': sum(len(c) for c in copies.values()),
              'unique_census': dict(counts), 'legacy_nonexact_scope_counts': dict(filter_counts),
              'retained_call_site_counts': dict(call_counts),
              'missing_reference_names': sorted(set(ref_sizes)-set(copies)),
              'candidate_only_bodies': extra, 'functions': records,
              'limits': ['Source-group labels are reconstructed paths, not proven original TU ownership.',
                         'ELF call targets establish neither return types nor read ages or reachability.',
                         'Indirect calls and ambiguous/interior local targets remain unresolved.',
                         'Legacy filter eligibility does not measure the union of historical investigations.',
                         'Direct tail transfers are not yet inventoried; no complete call-graph claim.',
                         'Archived manifests describe their recorded build, not the current working source.',
                         'Without an archived manifest, timestamps and source stability are checked; this is not original compilation provenance.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print('ledger: %d objects, %d/%d shared reference names, %d defining copies' %
          (len(manifest), len(records), len(ref_sizes), result['shared_defining_copies']))
    print('unique grades:', dict(counts), '; legacy nonexact scope:', dict(filter_counts))
    print('retained call sites:', dict(call_counts), '; candidate-only bodies:', len(extra))


if __name__ == '__main__':
    main()
