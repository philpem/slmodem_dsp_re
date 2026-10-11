#!/usr/bin/env python3
"""Audit complete baseline objects and index observable RTL transitions.

Transitions are our compiler's evidence, not the original compiler's state.
Repeated headers remain ambiguous; no emitted clone ownership is guessed.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks, fingerprint, store_splits
from gcc3_reload_trace import instructions


def core(header):
    # Resolve explicit [with ...] type bindings only for header nomination.
    binding = re.search(r' \[with (.*)\]$', header)
    if binding:
        header = header[:binding.start()]
        for item in binding[1].split(', '):
            key, value = item.split(' = ', 1)
            header = re.sub(r'\b'+re.escape(key)+r'\b', value, header)
    return re.sub(r'\s+', '', header.split('(', 1)[0])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--run', type=Path, action='append', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--scratch-manifest', type=Path, help='write the declared seven-TU/eight-control replay manifest')
    args = ap.parse_args()
    inventory = json.loads(args.inventory.read_text())
    valid, failed = {}, []
    verdict_count = 0
    for folder in args.run:
        result = json.loads((folder/'results.json').read_text())
        assert result['revision'] == inventory['revision']
        assert result['config'] == inventory['build_config']
        for family, record in result['families'].items():
            entry = record['cells']['baseline']
            if not entry.get('baseline_reproduced'):
                failed.append({'source': record['source_path'], 'directory': str(folder/family/'baseline'),
                               'compile_exit': entry['compile_exit']})
                continue
            source = record['source_path']
            obj = folder/family/'baseline/candidate.o'
            retained = d.ROOT/'build/tc_out'/(source.replace('/', '_')+'.o')
            assert obj.read_bytes() == retained.read_bytes()
            assert inspect(obj) == inspect(retained)
            assert set(entry['functions']) == set(d.b.sizes(str(obj)))
            verdict_count += len(entry['verdicts'])
            valid[source] = {'directory': obj.parent, 'object': obj}
    rows = []
    counts = Counter()
    windows = [('24.lreg', '25.greg'), ('27.flow2', '28.peephole2'),
               ('28.peephole2', '30.rnreg'), ('30.rnreg', '33.sched2')]
    for target in inventory['candidates']:
        source = target['source']
        row = {key: target[key] for key in ('symbol', 'demangled', 'grade', 'source')}
        if source not in valid:
            row['mapping'] = 'no valid baseline'
            counts[row['mapping']] += 1
            rows.append(row)
            continue
        folder = valid[source]['directory']
        row['baseline_directory'] = str(folder)
        name = re.sub(r'\s+', '', target['demangled'].split('(', 1)[0])
        dumps = {p.name.split('.', 1)[-1]: p for p in folder.iterdir()
                 if re.search(r'\.[0-9]{2}\.[a-z0-9_]+$', p.name)}
        # Filenames can have several periods, e.g. .cpp.27.flow2.
        dumps = {re.search(r'([0-9]{2}\.[a-z0-9_]+)$', p.name)[1]: p for p in dumps.values()}
        reference = dumps.get('27.flow2', dumps.get('01.rtl'))
        assert reference is not None
        headers = [part.splitlines()[0].strip() for part in reference.read_text().split(';; Function ')[1:]]
        possible = [h for h in headers if core(h).endswith(name)]
        row['matched_headers'] = possible
        if len(possible) != 1:
            row['mapping'] = 'ambiguous' if possible else 'unmapped'
            counts[row['mapping']] += 1
            rows.append(row)
            continue
        header = possible[0]
        row['mapping'] = 'unique header'
        counts[row['mapping']] += 1
        streams = {}
        row['windows'] = {}
        for before, after in windows:
            key = before+' -> '+after
            if before not in dumps or after not in dumps:
                row['windows'][key] = {'available': False}
                continue
            for stage in (before, after):
                if stage not in streams:
                    selected = chunks(dumps[stage], header)
                    assert len(selected) == 1, 'repeated header outside nominated window'
                    streams[stage] = instructions(selected[0])
            a, b = streams[before], streams[after]
            changed = [uid for uid in sorted(set(a)|set(b)) if fingerprint(a.get(uid)) != fingerprint(b.get(uid))]
            row['windows'][key] = {'available': True, 'changed_uids': changed,
                                   'before_uids': len(a), 'after_uids': len(b)}
            if (before, after) == ('27.flow2', '28.peephole2'):
                row['static_immediate_store_splits'] = store_splits(a, b)
        rows.append(row)
    assert len(rows) == inventory['candidate_count'] == 97
    # Known controls must be observed, never inferred from silence.
    fir = next(r for r in rows if r['symbol'] == '_ZN8FloatFIR5resetEv')
    assert fir['windows']['27.flow2 -> 28.peephole2']['changed_uids']
    iir = next(r for r in rows if r['symbol'] == '_iir_filter_create')
    # Zero scratch searches does NOT mean no peephole transformations.
    assert len(iir['windows']['27.flow2 -> 28.peephole2']['changed_uids']) == 8
    assert len(iir['windows']['28.peephole2 -> 30.rnreg']['changed_uids']) == 6
    assert core('void Scrambler<T, I>::reset(T) [with T = unsigned char, I = int]') == 'voidScrambler<unsignedchar,int>::reset'
    output = {'revision': inventory['revision'], 'valid_TUs': len(valid), 'failed_attempts': failed,
              'full_TU_body_verdicts': verdict_count, 'mapping_counts': dict(counts), 'targets': rows,
              'interpretation': 'observed retained transitions only; no original cursor/state or root cause inferred'}
    args.output.write_text(json.dumps(output, indent=2)+'\n')
    if args.scratch_manifest:
        sources = ('src/pump/v90/V90Phase4Modulator.cpp', 'src/pump/v90/V92Phase4Modulator.cpp',
                   'src/pump/v90/V90Phase3Modulator.cpp', 'src/pump/v90/V90MP.cpp',
                   'src/pump/v90/V92EchoCanceller.cpp', 'src/dsp/FloatFIR.cpp', 'src/v8/v8.c')
        manifest = [{'label': Path(source).stem, 'directory': str(valid[source]['directory']),
                     'compiler': 'cc1plus' if source.endswith('.cpp') else 'cc1',
                     'input': Path(source).stem+('.ii' if source.endswith('.cpp') else '.i')} for source in sources]
        repeat = dict(manifest[-1]); repeat['label'] = 'v8-repeat'; manifest.append(repeat)
        args.scratch_manifest.write_text(json.dumps(manifest, indent=2)+'\n')
    print(len(valid), 'raw-identical complete TUs /', verdict_count, 'function verdicts;', dict(counts))
    print(len(failed), 'failed attempts retained separately')
    for row in rows:
        if row['grade'] == 'REGALLOC':
            print(row['demangled'], row['mapping'],
                  {k: len(v.get('changed_uids', [])) if v['available'] else None for k,v in row.get('windows', {}).items()})


if __name__ == '__main__':
    main()
