#!/usr/bin/env python3
"""Bound the V34 fixture-probe removal against its complete period owner.

Replay the unchanged TU before removing the reconstruction-only export. No
profile changes or forced emission of the fully inlined t3c_txblock shim.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import playbook_small_patterns as d
from recovery_generator_separation import separate, named_data
from recovery_fixedrc_apparatus import allocated
from recovery_fax_debug_audit import Object
from recovery_debug_boundary_audit import strings
from recovery_residual_ledger import call_sites, function_owners
from narrow_call_boundary_screen import bodies
from elftools.elf.elffile import ELFFile

SOURCE = 'src/pump/v34/V34hshak.c'
PROBE = 'v34handshak_txblock'


def owner_bodies(path):
    """Audit local E8 displacement fields by unique, existing ELF callee.

    This is a baseline/candidate collateral audit, not a byteident grade.
    Ordinary relocation fields remain independently checked at each position.
    """
    result = Object(path).canonical()[0]
    positions, _ = function_owners(path)
    calls = call_sites(path, bodies(path))
    edges = {}
    for name, raw in result.items():
        code = bytearray(raw)
        start = next(iter(positions[name]))[1]
        local = {}
        for call in calls[name]:
            if call['kind'] != 'local-direct':
                assert call['kind'] in ('relocated', 'indirect'), call
                continue
            at = int(call['address'], 16)-start
            assert code[at] == 0xe8 and at+5 <= len(code)
            local[at+1] = call['target']
            code[at+1:at+5] = b'\0'*4
        result[name] = bytes(code)
        edges[name] = local
    return result, edges


def inspect(before, after):
    old, new = Object(before).canonical(), Object(after).canonical()
    oldnames, newnames = set(old[0]), set(new[0])
    shared = sorted(oldnames & newnames)
    oldbodies, oldedges = owner_bodies(before)
    newbodies, newedges = owner_bodies(after)
    oldalloc, newalloc = allocated(before), allocated(after)
    before_sizes, after_sizes = d.b.sizes(str(before)), d.b.sizes(str(after))
    def metadata(path):
        obj = Object(path)
        return {s.name: (s['st_info']['type'], s['st_info']['bind'], s['st_other']['visibility'],
                         obj.sections[s['st_shndx']][0] if isinstance(s['st_shndx'], int) else s['st_shndx'],
                         s['st_size']) for s in obj.symbols
                if s.name and s['st_info']['type'] not in ('STT_FILE', 'STT_SECTION')}
    oldmeta, newmeta = metadata(before), metadata(after)
    return {
        'surviving_bodies': len(shared),
        'removed_functions': sorted(oldnames-newnames),
        'added_functions': sorted(newnames-oldnames),
        'changed_masked_raw_bodies': [n for n in shared if old[0][n] != new[0][n]],
        'changed_owner_bodies': [n for n in shared if oldbodies[n] != newbodies[n]],
        'changed_positioned_local_calls': [n for n in shared if oldedges[n] != newedges[n]],
        'surviving_local_call_fields_checked': sum(len(newedges[n]) for n in shared),
        'changed_positioned_relocations': [n for n in shared if
            d.b.body(str(before), n)[1] != d.b.body(str(after), n)[1]],
        'changed_resolved_consumers': [n for n in old[3].keys() | new[3].keys()
            if n != PROBE and old[3].get(n) != new[3].get(n)],
        'removed_bindings': sorted(set(old[2])-set(new[2])),
        'added_bindings': sorted(set(new[2])-set(old[2])),
        'surviving_symbol_metadata_equal': {n: v for n, v in oldmeta.items() if n != PROBE} == newmeta,
        'named_data_equal': named_data(before) == named_data(after),
        'nontext_allocated_equal': {n: v for n, v in oldalloc.items() if n != '.text'} ==
                                 {n: v for n, v in newalloc.items() if n != '.text'},
        'diagnostic_call_multisets_equal': all(Counter(r['text'] for r in strings(before, n)) ==
                                              Counter(r['text'] for r in strings(after, n)) for n in shared),
        'size_changes': {n: [before_sizes[n], after_sizes[n]] for n in shared
                         if before_sizes[n] != after_sizes[n]},
        'text_bytes_removed_including_alignment': len(oldalloc['.text'][0])-len(newalloc['.text'][0]),
    }


def require_safe(row):
    assert row['removed_functions'] == [PROBE] and not row['added_functions'], row
    for key in ('changed_owner_bodies', 'changed_positioned_local_calls', 'changed_positioned_relocations',
                'changed_resolved_consumers', 'added_bindings', 'size_changes'):
        assert not row[key], (key, row[key])
    assert row['removed_bindings'] == [(PROBE, 'STB_GLOBAL', 'STT_FUNC', 'STV_DEFAULT')]
    for key in ('named_data_equal', 'nontext_allocated_equal', 'diagnostic_call_multisets_equal',
                'surviving_symbol_metadata_equal'):
        assert row[key], key


def refusal_controls(before, after, out):
    """Change real coefficient, jump-table, local-call and diagnostic inputs."""
    raw = bytearray(after.read_bytes())
    with after.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        coefficient = next(s for s in tab.iter_symbols() if s.name == 'c2400_')
        coefficient_at = elf.get_section(coefficient['st_shndx'])['sh_offset']+coefficient['st_value']
        table = elf.get_section_by_name('.rodata')
        jump = next(elf.get_section_by_name('.rel.rodata').iter_relocations())
        jump_at = table['sh_offset']+jump['r_offset']
        owner = next(s for s in tab.iter_symbols() if s.name == 'v34handshak')
        site = next(c for c in call_sites(after, bodies(after))[owner.name] if c['kind'] == 'local-direct')
        local_at = elf.get_section(owner['st_shndx'])['sh_offset']+int(site['address'], 16)+1
        diagnostic = next(s for s in elf.iter_sections() if s.name.startswith('.rodata.str'))
        diagnostic_at = diagnostic['sh_offset']
    tests = []
    for label, at, width in [('coefficient', coefficient_at, 1), ('jump-destination', jump_at, 4),
                              ('local-callee', local_at, 4), ('diagnostic', diagnostic_at, 1)]:
        contents = bytearray(raw)
        value = int.from_bytes(contents[at:at+width], 'little')
        contents[at:at+width] = ((value+1) % (1 << (width*8))).to_bytes(width, 'little')
        path = out/(label+'.o')
        path.write_bytes(contents)
        try:
            require_safe(inspect(before, path))
        except (AssertionError, ValueError):
            tests.append(label)
            continue
        raise AssertionError('audit failed to detect '+label)
    require_safe(inspect(before, after))
    print('V34 probe audit controls: 5 passed, 0 failed (four changed inputs, unchanged)', flush=True)
    return {'changed_inputs_rejected': tests, 'unchanged_passed': True, 'checks': 5}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--revision', default='239b234c0')
    parser.add_argument('--working', action='store_true')
    args = parser.parse_args()
    root = d.ROOT
    out = root/'build/fidelity14-v34-probe'
    out.mkdir(parents=True, exist_ok=True)
    config = (args.baseline/'.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/'+f
             if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    headers = subprocess.check_output(['rg', '--files', 'include', 'src', '-g', '*.h', '-g', '!re/**'], cwd=root, text=True).splitlines()
    hashes = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in headers}
    historical = out/'historical-headers/dsplib/v34hshak.h'
    historical.parent.mkdir(parents=True, exist_ok=True)
    historical.write_bytes(subprocess.check_output(['git', 'show', args.revision+':include/dsplib/v34hshak.h'], cwd=root))
    identity = d.tc.docker_prefix(image, root, out, True)+['/bin/sh', '-c',
        'export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; command -v gcc; gcc --version; '
        'selected_as=$(gcc -print-prog-name=as); "$selected_as" --version']
    with (out/'identity.log').open('w') as log:
        subprocess.run(identity, stdout=log, stderr=subprocess.STDOUT, check=True)
    source = subprocess.check_output(['git', 'show', args.revision+':'+SOURCE], cwd=root, text=True)
    removed, _ = separate(source, PROBE)
    cells = [('baseline', source), ('probe-removed', removed)]
    if args.working:
        cells.append(('working-source', (root/SOURCE).read_text()))
    result = {'revision': args.revision, 'config': config, 'identity_command': identity,
              'header_sha256': hashes, 'cells': {}}
    before = None
    for label, contents in cells:
        folder = out/label
        folder.mkdir(exist_ok=True)
        (folder/'V34hshak.c').write_text(contents)
        virtual = '/work/'+label
        compile_flags = flags if label == 'working-source' else ['-I/work/historical-headers']+flags
        shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, compile_flags+['-v', '-da'],
                                  virtual+'/candidate.o', virtual+'/V34hshak.c')
        command = d.tc.docker_prefix(image, root, out, True)+['/bin/sh', '-c', 'cd '+shlex.quote(virtual)+' && '+shell]
        with (folder/'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        obj = folder/'candidate.o'
        functions = d.b.sizes(str(obj))
        row = {'command': command, 'source_sha256': hashlib.sha256(contents.encode()).hexdigest(),
               'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
               'functions': functions,
               'verdicts': {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n))
                            for n in sorted(set(functions) & set(d.b.sizes(d.b.BLOB)))}}
        callgraph = (folder/'V34hshak.c.00.cgraph').read_text()
        clones = re.findall(r'^t3c_txblock:.*cloned (\d+)x$', callgraph, re.M)
        assert clones and 't3c_txblock' not in functions and 't3m_txblock' in functions
        row['t3c_inlined_clones'] = int(clones[-1])
        row['standalone_t3c_not_emitted'] = True
        result['cells'][label] = row
        if label == 'baseline':
            assert obj.read_bytes() == (args.baseline/(SOURCE.replace('/', '_')+'.o')).read_bytes(), 'raw baseline drift'
            row['raw_baseline_reproduced'] = True
            before = obj
        else:
            row.update(inspect(before, obj))
            old = result['cells']['baseline']['verdicts']
            row['exact_gains'] = [n for n, v in row['verdicts'].items() if v[0] == 'EXACT' and old[n][0] != 'EXACT']
            row['exact_losses'] = [n for n, v in old.items() if v[0] == 'EXACT' and row['verdicts'][n][0] != 'EXACT']
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
        print(label, len(functions), 'functions; exact', sum(v[0] == 'EXACT' for v in row['verdicts'].values()), '/', len(row['verdicts']), flush=True)
        if label != 'baseline':
            print('full-owner changes:', {k: v for k, v in row.items() if k.startswith('changed_') or k.endswith('_equal')}, flush=True)
            if args.working:
                require_safe(row)
                assert not row['exact_gains'] and not row['exact_losses']
                if label == 'working-source':
                    assert obj.read_bytes() == (out/'probe-removed/candidate.o').read_bytes()
                    row['raw_removal_candidate_reproduced'] = True
                    result['controls'] = refusal_controls(before, obj, out)
                    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    assert all(hashlib.sha256((root/p).read_bytes()).hexdigest() == value for p, value in hashes.items())


if __name__ == '__main__':
    main()
