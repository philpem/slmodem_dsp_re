#!/usr/bin/env python3
"""Broaden existing expansion detectors to all retained source TUs and sizes."""
from collections import Counter
import hashlib
import json
import subprocess
import playbook_small_patterns as d
from expansion_wide_screen import calls
from fixed_index_screen import functions, backedges
from data_literal_expansion_screen import SCOPE as PRIOR_DATA_SCOPE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def deficits(original, retained):
    return {n: [count, retained[n]] for n, count in original.items()
            if count >= 2 and retained[n] <= 1}


def main():
    census = d.ROOT / 'build/byteident-residual-value-owner.json'
    saved = json.loads(census.read_text())
    exact = set(saved['exact_symbols'])
    original_sizes = d.b.sizes(d.b.BLOB)
    original_edges = backedges(d.b.BLOB)
    report = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=d.ROOT, text=True).strip(),
              'census_sha256': sha(census), 'config': (d.ROOT / 'build/tc_out/.build-config').read_text(),
              'counts': Counter(), 'source_hashes': {}, 'object_hashes': {}, 'nominations': []}
    report['source_baseline'] = '7edfb734'
    subprocess.run(['git', 'diff', '--exit-code', '7edfb734', '--', 'src', 'include',
                    'tools/toolchain/period.mk'], cwd=d.ROOT, check=True)
    assert saved['exact'] == 1079
    for source in sorted((d.ROOT / 'src').rglob('*')):
        if source.suffix not in ('.c', '.cpp'):
            continue
        relative = str(source.relative_to(d.ROOT))
        obj = d.ROOT / 'build/tc_out' / (relative.replace('/', '_') + '.o')
        assert obj.exists(), relative
        report['counts']['translation_units'] += 1
        report['source_hashes'][relative] = sha(source)
        report['object_hashes'][obj.name] = sha(obj)
        loops, unsupported, unmapped, total = functions(source.read_text())
        report['counts'].update({'lexical_for_headers': total, 'recognized_fixed_headers': len(loops),
                                 'unsupported_headers': unsupported, 'unmapped_headers': unmapped})
        own_edges = backedges(obj)
        names = d.b.sizes(str(obj))
        demangled = subprocess.check_output(['c++filt'], input='\n'.join(names) + '\n', text=True).splitlines()
        owners = {n: dem.split('(', 1)[0] for n, dem in zip(names, demangled)}
        tu_calls = {}
        for name in names:
            own_calls, excluded = calls(obj, name)
            tu_calls[name] = own_calls
            report['counts']['excluded_retained_calls'] += sum(excluded.values())
        for name, size in names.items():
            report['counts']['emitted_bodies'] += 1
            if name not in original_sizes:
                report['counts']['without_original'] += 1
                continue
            if name in exact:
                report['counts']['exact_excluded'] += 1
                continue
            report['counts']['eligible_nonexact_bodies'] += 1
            paired_loops = [loop for loop in loops if loop['owner'] == owners[name]]
            if paired_loops:
                report['counts']['fixed_loop_bodies'] += 1
            original_calls, excluded = calls(d.b.BLOB, name)
            report['counts']['excluded_original_calls'] += sum(excluded.values())
            missing = deficits(original_calls, tu_calls[name])
            fewer_edges = paired_loops and len(original_edges[name]) < len(own_edges[name])
            if not missing and not fewer_edges:
                continue
            prior_call_scope = ((source.suffix == '.cpp' or relative.startswith('src/dsp/'))
                                and not relative.startswith('src/pump/v34/') and original_sizes[name] <= 4096)
            prior_call_scope |= relative.startswith(PRIOR_DATA_SCOPE) and original_sizes[name] <= 800
            new_calls = bool(missing) and not prior_call_scope
            new_loops = bool(fewer_edges) and (relative.startswith('src/pump/v34/') or original_sizes[name] > 4096)
            row = {'source': relative, 'symbol': name, 'owner': owners[name],
                   'blob_bytes': original_sizes[name], 'ours_bytes': size, 'loops': paired_loops,
                   'overloads': sum(o == owners[name] for o in owners.values()),
                   'repeat_differences': missing, 'blob_calls': dict(original_calls),
                   'ours_calls': dict(tu_calls[name]), 'blob_backedges': original_edges[name],
                   'ours_backedges': own_edges[name],
                   'retained_direct_callers_in_TU': [n for n, targets in tu_calls.items() if targets[name]],
                   'new_call_scope': new_calls, 'new_loop_scope': new_loops,
                   'new_scope': new_calls or new_loops}
            report['nominations'].append(row)
        if report['counts']['translation_units'] % 50 == 0:
            print(report['counts']['translation_units'], 'C/C++ TUs screened', flush=True)
    report['counts']['assembly_TUs_excluded'] = len(list((d.ROOT / 'src').rglob('*.S')))
    output = d.ROOT / 'build/helper-caller-screen.json'
    report['controls'] = {'status': 'not yet checked'}
    output.write_text(json.dumps(report, indent=2) + '\n')
    assert report['counts']['translation_units'] == 299 and report['counts']['assembly_TUs_excluded'] == 1
    positive = next(r for r in report['nominations'] if r['symbol'] == '_ZN11V92Precoder5resetEP16V92MappingParams')
    assert len(positive['blob_backedges']) == 0 and len(positive['ours_backedges']) == 2
    assert [x['bound'] for x in positive['loops']] == [6, 12]
    assert deficits(Counter({'__moddi3': 5, '__divdi3': 6}), Counter({'__moddi3': 1, '__divdi3': 2})) == {'__moddi3': [5, 1]}
    target = '_ZN21V90ConstellationPower21calcModulusParametersEP16V90MappingParams'
    original, _ = calls(d.b.BLOB, target)
    retained, _ = calls(d.ROOT / 'build/tc_out/src_pump_v90_V90ConstellationPower.cpp.o', target)
    assert original == retained and not deficits(original, retained)
    report['controls'] = {'closed_fixed_loop_positive': True, 'historical_count_positive': True,
                          'live_exact_call_negative': True}
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(dict(report['counts']))
    print('3 detector controls pass;', len(report['nominations']), 'nominations')
    for row in report['nominations']:
        print('NEW' if row['new_scope'] else 'prior scope', row['owner'], row['blob_bytes'],
              row['repeat_differences'], len(row['blob_backedges']), len(row['ours_backedges']))


if __name__ == '__main__':
    main()
