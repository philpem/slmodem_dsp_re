#!/usr/bin/env python3
"""Measure complete period TUs with reconstructed-only generators removed.

Three cells per TU: unchanged raw reproduction, generator removal only, and
the final working source (including comment/prototype cleanup).
No compiler/profile palette. Production source remains untouched by this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from recovery_fax_debug_audit import Object

FAMILIES = {
    'src/dsp/fpm_log10.c': 'FPM_log10_table_generate',
    'src/dsp/fpm_sqrt.c': 'FPM_sqrt_table_generate',
    'src/dsp/fpm_div.c': 'FPM_div_table_generate',
    'src/dsp/FP_math.c': 'FP_Pow_coefficient_generate',
}
REMOVED_CONSTANTS = {
    'fpm_log10': '0000003800000047',
    'fpm_sqrt': '0000803b00000047',
    'fpm_div': '',
    'FP_math': '00008046',
}


def separate(source, name):
    start = source.index('\n'+name+'(') + 1
    start = source.rfind('\n', 0, start-1) + 1
    end = source.index('\n}\n', start) + 3
    definition = source[start:end]
    assert definition.count(name+'(') == 1
    return source[:start]+source[end:], definition


def named_data(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        return {s.name: (s['st_info']['bind'], s['st_size'],
                        elf.get_section(s['st_shndx']).data()[s['st_value']:s['st_value']+s['st_size']].hex())
                for s in elf.get_section_by_name('.symtab').iter_symbols()
                if s['st_info']['type'] == 'STT_OBJECT' and isinstance(s['st_shndx'], int)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--revision', default='dcc81e6f')
    args = parser.parse_args()
    root = d.ROOT
    out = root/'build/fidelity12-generator-separation'
    out.mkdir(parents=True, exist_ok=True)
    config = (args.baseline/'.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/'+f
             if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    identity = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '-c',
        'export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; gcc --version; '
        'compiler_as=$(gcc -print-prog-name=as); "$compiler_as" --version']
    with (out/'identity.log').open('w') as log:
        subprocess.run(identity, stdout=log, stderr=subprocess.STDOUT, check=True)
    headers = subprocess.check_output(['rg', '--files', 'include', 'src', '-g', '*.h'],
                                      cwd=root, text=True).splitlines()
    header_hashes = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in headers}
    result = {'revision': args.revision, 'config': config, 'identity_command': identity,
              'header_sha256': header_hashes, 'families': {}}
    for path, name in FAMILIES.items():
        source = subprocess.check_output(['git', 'show', args.revision+':'+path], cwd=root, text=True)
        removed, _ = separate(source, name)
        assert name not in d.b.sizes(d.b.BLOB), ('generator unexpectedly in reference', name)
        family = Path(path).stem
        report = {'removed_generator': name, 'cells': {}}
        result['families'][family] = report
        before = None
        for label, contents in [('baseline', source), ('generator-in-fixture', removed),
                                ('working-source', (root/path).read_text())]:
            folder = out/family/label
            folder.mkdir(parents=True, exist_ok=True)
            (folder/Path(path).name).write_text(contents)
            virtual = '/work/'+family+'/'+label
            shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, flags+['-v'],
                                       virtual+'/candidate.o', virtual+'/'+Path(path).name)
            command = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '-c', shell]
            with (folder/'compile.log').open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
            obj = folder/'candidate.o'
            entry = {'command': command, 'source_sha256': hashlib.sha256(contents.encode()).hexdigest(),
                     'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                     'functions': sorted(d.b.sizes(str(obj))),
                     'verdicts': {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n))
                                  for n in sorted(set(d.b.sizes(str(obj))) & set(d.b.sizes(d.b.BLOB)))}}
            report['cells'][label] = entry
            if label == 'baseline':
                retained = args.baseline/(path.replace('/', '_')+'.o')
                assert obj.read_bytes() == retained.read_bytes(), (family, 'raw baseline mismatch')
                entry['raw_baseline_reproduced'] = True
                before = obj
                continue
            old = report['cells']['baseline']
            assert set(entry['functions']) == set(old['functions']) - {name}
            entry['changed_surviving_bodies'] = [n for n in entry['functions']
                if d.b.body(str(before), n) != d.b.body(str(obj), n)]
            entry['exact_gains'] = sorted(n for n, v in entry['verdicts'].items()
                                        if v[0] == 'EXACT' and old['verdicts'][n][0] != 'EXACT')
            entry['exact_losses'] = sorted(n for n, v in old['verdicts'].items()
                                         if v[0] == 'EXACT' and entry['verdicts'][n][0] != 'EXACT')
            a, b = Object(before).canonical(), Object(obj).canonical()
            entry['named_data_unchanged'] = named_data(before) == named_data(obj)
            entry['binding_removed'] = sorted(set(a[2])-set(b[2]))
            entry['binding_added'] = sorted(set(b[2])-set(a[2]))
            entry['nontext_payload_before'] = {k: v.hex() for k, v in a[1].items()}
            entry['nontext_payload_after'] = {k: v.hex() for k, v in b[1].items()}
            entry['changed_surviving_consumers'] = [n for n in a[3].keys() | b[3].keys()
                if n != name and a[3].get(n) != b[3].get(n)]
            expected_removed = {(name, 'STB_GLOBAL', 'STT_FUNC', 'STV_DEFAULT')}
            if family in ('fpm_log10', 'fpm_sqrt'):
                expected_removed.add(('log10' if family == 'fpm_log10' else 'sqrt',
                                      'STB_GLOBAL', 'STT_NOTYPE', 'STV_DEFAULT'))
            assert set(a[2])-set(b[2]) == expected_removed and not set(b[2])-set(a[2])
            assert entry['named_data_unchanged']
            assert not entry['changed_surviving_bodies'] and not entry['changed_surviving_consumers']
            assert not entry['exact_gains'] and not entry['exact_losses']
            before_data, after_data = dict(a[1]), dict(b[1])
            constants = bytes.fromhex(REMOVED_CONSTANTS[family])
            if constants:
                assert before_data.pop('.rodata.cst4') == constants
                assert '.rodata.cst4' not in after_data
            assert before_data == after_data, (family, 'unexpected nontext payload delta')
            if label == 'working-source':
                assert obj.read_bytes() == (out/family/'generator-in-fixture/candidate.o').read_bytes()
                entry['raw_removal_candidate_reproduced'] = True
            print(family, 'original body changes', entry['changed_surviving_bodies'],
                  'exact gains/losses', entry['exact_gains'], entry['exact_losses'], flush=True)
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    assert all(hashlib.sha256((root/p).read_bytes()).hexdigest() == value
               for p, value in header_hashes.items()), 'headers changed during compilation'


if __name__ == '__main__':
    main()
