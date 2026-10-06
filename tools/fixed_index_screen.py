#!/usr/bin/env python3
"""Bounded lexical fixed-loop/backedge triage; nominations need graph review."""
import playbook_small_patterns as d
import hashlib
import json
import re
import subprocess
from pathlib import Path

LOOP = re.compile(r'\bfor\s*\(([^;{}]*);([^;{}]*);([^;{}]*)\)')
HEADER = re.compile(r'\b([\w:~]+)\s*\([^;{}]*\)\s*(?:const\s*)?\{')


def functions(source):
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                   lambda m: re.sub(r'[^\n]', ' ', m[0]), source, flags=re.S)
    spans = []
    for match in HEADER.finditer(clean):
        if match[1] in ('for', 'while', 'if', 'switch', 'catch'):
            continue
        start = match.end()-1
        depth, end = 1, start+1
        while end < len(clean) and depth:
            depth += (clean[end] == '{')-(clean[end] == '}')
            end += 1
        if depth:
            raise ValueError('unbalanced function body')
        spans.append((match[1], start, end))
    loops = []
    unsupported = unmapped = 0
    for match in LOOP.finditer(clean):
        bound = re.search(r'\b([A-Za-z_]\w*)\s*(<=|<)\s*(\d+)[uUlL]*\s*$', match[2])
        if not bound or not 2 <= int(bound[3]) <= 32:
            unsupported += 1
            continue
        owners = [(name, start, end) for name, start, end in spans if start < match.start() < end]
        if len(owners) != 1:
            unmapped += 1
            continue
        loops.append({'owner': owners[0][0], 'line': source.count('\n', 0, match.start())+1,
                      'header': match[0], 'bound': int(bound[3]), 'comparison': bound[2]})
    return loops, unsupported, unmapped, len(list(LOOP.finditer(clean)))


def backedges(path):
    text = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', str(path)], text=True)
    result, current = {}, None
    for line in text.splitlines():
        match = re.match(r'^([0-9a-f]+) <(.+)>:$', line)
        if match:
            current = match[2]
            result[current] = []
            continue
        match = re.match(r'^\s*([0-9a-f]+):\s+(j\w+|loop\w*)\s+([0-9a-f]+)\s+<', line)
        if current is not None and match and match[2] not in ('jmp', 'ljmp'):
            address, target = int(match[1], 16), int(match[3], 16)
            if target < address:
                result[current].append({'address': address, 'target': target, 'mnemonic': match[2]})
    return result


def main():
    root = d.ROOT
    baseline_path = root/'build/baseline-byteident.json'
    baseline = json.loads(baseline_path.read_text())
    assert baseline['exact'] == 1070
    exact = set(baseline['exact_symbols'])
    sizes = d.b.sizes(d.b.BLOB)
    original = backedges(d.b.BLOB)
    report = {'revision': '548b5edd', 'objects': 0, 'emitted_bodies': 0,
              'eligible_nonexact_bodies': 0, 'lexical_for_headers': 0,
              'unsupported_headers': 0, 'unmapped_headers': 0,
              'recognized_fixed_headers': 0, 'fixed_loop_bodies': [], 'nominations': [],
              'provenance': {'baseline_json_sha256': hashlib.sha256(baseline_path.read_bytes()).hexdigest(),
                             'build_config': (root/'build/production-before/.build-config').read_text(),
                             'source_sha256': {}, 'object_sha256': {}}}
    for path in sorted((root/'src').rglob('*')):
        relative = str(path.relative_to(root))
        if path.suffix not in ('.c', '.cpp') or relative.startswith('src/pump/v34/'):
            continue
        obj = root/'build/production-before'/(relative.replace('/', '_')+'.o')
        if not obj.exists():
            raise AssertionError(('missing scope object', relative))
        source = path.read_bytes()
        expected = subprocess.check_output(['git', 'show', '548b5edd:'+relative], cwd=root)
        assert source == expected, ('source drift from declared baseline', relative)
        report['provenance']['source_sha256'][relative] = hashlib.sha256(source).hexdigest()
        report['provenance']['object_sha256'][obj.name] = hashlib.sha256(obj.read_bytes()).hexdigest()
        names = d.b.sizes(str(obj))
        report['objects'] += 1
        report['emitted_bodies'] += len(names)
        loops, unsupported, unmapped, count = functions(path.read_text())
        report['lexical_for_headers'] += count
        report['unsupported_headers'] += unsupported
        report['unmapped_headers'] += unmapped
        report['recognized_fixed_headers'] += len(loops)
        labels = list(names)
        demangled = subprocess.check_output(['c++filt'], input='\n'.join(labels)+'\n', text=True).splitlines()
        owners = {name: label.split('(', 1)[0] for name, label in zip(labels, demangled)}
        own = backedges(obj)
        for name in names:
            if name not in sizes or name in exact or sizes[name] > 4096:
                continue
            report['eligible_nonexact_bodies'] += 1
            matched = [loop for loop in loops if loop['owner'] == owners[name]]
            if not matched:
                continue
            assert name in original and name in own, ('missing disassembly', name)
            row = {'source': relative, 'symbol': name, 'owner': owners[name],
                   'blob_bytes': sizes[name], 'ours_bytes': names[name], 'loops': matched,
                   'overloads': sum(owner == owners[name] for owner in owners.values()),
                   'blob_backedges': original[name], 'ours_backedges': own[name]}
            report['fixed_loop_bodies'].append(row)
            if len(original[name]) < len(own[name]):
                report['nominations'].append(row)
    control = '_ZN11V92Precoder5resetEP16V92MappingParams'
    positive = [r for r in report['nominations'] if r['symbol'] == control]
    assert len(positive) == 1
    assert len(positive[0]['blob_backedges']) == 0 and len(positive[0]['ours_backedges']) == 2
    assert [r['bound'] for r in positive[0]['loops']] == [6, 12]
    report['known_closed_positive_fired'] = True
    (root/'build/fixed-index-screen.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('fixed_loop_bodies', 'nominations', 'provenance')}, indent=2))
    for row in report['nominations']:
        print(row['source'], row['owner'], row['blob_bytes'], row['ours_bytes'],
              len(row['blob_backedges']), len(row['ours_backedges']), [l['line'] for l in row['loops']])


if __name__ == '__main__':
    main()
