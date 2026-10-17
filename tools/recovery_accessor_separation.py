#!/usr/bin/env python3
"""Control fixture-only accessor separation using unchanged complete period TUs.

Two cells per family: raw baseline reproduction and accessor removal. Optional
--working verifies retained source including comments/prototype cleanup. Every
surviving body, relocation consumer, named datum and export binding is checked.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import playbook_small_patterns as d
from recovery_generator_separation import separate, named_data
from recovery_fax_debug_audit import Object

FAMILIES = {
    'src/dsp/fpm_log10.c': ('FPM_log10_table_derived', 'FPM_log10_table_entry', 'FPM_log10_table_size'),
    'src/dsp/fpm_sqrt.c': ('FPM_sqrt_table_entry', 'FPM_sqrt_table_size'),
    'src/dsp/fpm_div.c': ('FPM_div_table_entry',),
    'src/dsp/fpm_phasor.c': ('FPM_phasor_cos_entry', 'FPM_phasor_sin_entry'),
    'src/dsp/FP_math.c': ('FP_Pow_coefficient',),
    'src/service/Cidmtd.c': ('CID_MTD_coeff',),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--revision', default='13f477a9')
    parser.add_argument('--working', action='store_true')
    args = parser.parse_args()
    root = d.ROOT
    out = root/'build/fidelity13-helper-separation'
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
    headers = subprocess.check_output(['rg', '--files', 'include', 'src', '-g', '*.h', '-g', '!re/**'], cwd=root, text=True).splitlines()
    header_hashes = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in headers}
    result = {'revision': args.revision, 'config': config, 'identity_command': identity,
              'header_sha256': header_hashes, 'families': {}}
    for path, names in FAMILIES.items():
        source = subprocess.check_output(['git', 'show', args.revision+':'+path], cwd=root, text=True)
        removed = source
        for name in names:
            removed, _ = separate(removed, name)
            assert name not in d.b.sizes(d.b.BLOB), name
        family = Path(path).stem
        report = {'removed_accessors': names, 'cells': {}}
        result['families'][family] = report
        cells = [('baseline', source), ('accessors-in-fixture', removed)]
        if args.working:
            cells += [('working-source', (root/path).read_text())]
        for label, contents in cells:
            folder = out/family/label
            folder.mkdir(parents=True, exist_ok=True)
            (folder/Path(path).name).write_text(contents)
            virtual = '/work/'+family+'/'+label
            shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, flags+['-v'], virtual+'/candidate.o', virtual+'/'+Path(path).name)
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
            assert set(entry['functions']) == set(old['functions']) - set(names)
            a, b = Object(before).canonical(), Object(obj).canonical()
            entry['changed_surviving_bodies'] = [n for n in entry['functions'] if a[0][n] != b[0][n]]
            entry['changed_surviving_consumers'] = [n for n in a[3].keys() | b[3].keys()
                if n not in names and a[3].get(n) != b[3].get(n)]
            entry['changed_positioned_relocations'] = [n for n in entry['functions']
                if d.b.body(str(before), n)[1] != d.b.body(str(obj), n)[1]]
            entry['exact_gains'] = sorted(n for n, v in entry['verdicts'].items()
                                        if v[0] == 'EXACT' and old['verdicts'][n][0] != 'EXACT')
            entry['exact_losses'] = sorted(n for n, v in old['verdicts'].items()
                                         if v[0] == 'EXACT' and entry['verdicts'][n][0] != 'EXACT')
            entry['named_data_unchanged'] = named_data(before) == named_data(obj)
            entry['nontext_payload_unchanged'] = a[1] == b[1]
            entry['binding_removed'] = sorted(set(a[2])-set(b[2]))
            entry['binding_added'] = sorted(set(b[2])-set(a[2]))
            assert set(a[2])-set(b[2]) == {(n, 'STB_GLOBAL', 'STT_FUNC', 'STV_DEFAULT') for n in names}
            assert not entry['binding_added']
            assert entry['named_data_unchanged'] and entry['nontext_payload_unchanged']
            assert not entry['changed_surviving_bodies'] and not entry['changed_surviving_consumers']
            assert not entry['changed_positioned_relocations']
            assert not entry['exact_gains'] and not entry['exact_losses']
            if label == 'working-source':
                assert obj.read_bytes() == (out/family/'accessors-in-fixture/candidate.o').read_bytes()
                entry['raw_removal_candidate_reproduced'] = True
            print(family, label, len(entry['functions']), 'surviving bodies unchanged; exact gains/losses', entry['exact_gains'], entry['exact_losses'], flush=True)
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    assert all(hashlib.sha256((root/p).read_bytes()).hexdigest() == value for p, value in header_hashes.items())


if __name__ == '__main__':
    main()
