#!/usr/bin/env python3
"""Nominate indirect call deficits and header/macro fixed-loop graph differences."""
import ast
from collections import Counter
import hashlib
import json
import operator
import re
import subprocess
import playbook_small_patterns as d
from fixed_index_screen import HEADER, backedges

OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
       ast.LShift: operator.lshift, ast.RShift: operator.rshift,
       ast.BitOr: operator.or_, ast.BitAnd: operator.and_}


def constant(text):
    text = re.sub(r'\b(0x[0-9a-fA-F]+|\d+)[uUlL]+\b', r'\1', text)
    def visit(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
            return (-1 if isinstance(node.op, ast.USub) else 1) * visit(node.operand)
        if isinstance(node, ast.BinOp) and type(node.op) in OPS:
            left, right = visit(node.left), visit(node.right)
            if isinstance(node.op, (ast.LShift, ast.RShift)) and not 0 <= right <= 16:
                raise ValueError('shift outside bounded expression domain')
            result = OPS[type(node.op)](left, right)
            if abs(result) > 65536:
                raise ValueError('large constant')
            return result
        raise ValueError('not a supported constant expression')
    try:
        return visit(ast.parse(text.strip(), mode='eval').body)
    except (SyntaxError, ValueError):
        return None


def mask(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|^\s*#[^\n]*',
                  lambda m: re.sub(r'[^\n]', ' ', m[0]), text, flags=re.S | re.M)


def loops(text):
    clean = mask(text)
    spans = []
    for match in HEADER.finditer(clean):
        if match[1] in ('for', 'while', 'if', 'switch', 'catch'):
            continue
        start, end, depth = match.end() - 1, match.end(), 1
        while end < len(clean) and depth:
            depth += (clean[end] == '{') - (clean[end] == '}')
            end += 1
        if not depth:
            spans.append((match[1], start, end))
    origins, filename, line = [], None, 0
    for physical in text.splitlines():
        marker = re.match(r'^#\s+(\d+)\s+"([^"]+)"', physical)
        if marker:
            line, filename = int(marker[1]), marker[2]
        origins.append((filename, line))
        if not marker:
            line += 1
    found, counts = [], Counter()
    for match in re.finditer(r'\bfor\s*\(', clean):
        counts['for_headers'] += 1
        end, depth = match.end(), 1
        separators = []
        while end < len(clean) and depth:
            if clean[end] == ';' and depth == 1:
                separators.append(end)
            depth += (clean[end] == '(') - (clean[end] == ')')
            end += 1
        if depth or len(separators) != 2:
            counts['unsupported'] += 1
            continue
        a, z = separators
        init, cond, step = clean[match.end():a], clean[a+1:z], clean[z+1:end-1]
        test = re.fullmatch(r'\s*(\w+)\s*(<=|<)\s*(.*?)\s*', cond, re.S)
        if not test:
            counts['unsupported'] += 1
            continue
        variable = test[1]
        initial = re.search(r'\b' + re.escape(variable) + r'\s*=\s*(.*?)\s*$', init, re.S)
        first, bound = constant(initial[1]) if initial else None, constant(test[3])
        update = re.sub(r'\s+', '', step)
        if first is None or bound is None or update not in (variable+'++', '++'+variable, variable+'+=1'):
            counts['unsupported'] += 1
            continue
        iterations = bound - first + (test[2] == '<=')
        if not 2 <= iterations <= 64:
            counts['outside_iteration_domain'] += 1
            continue
        owner = [name for name, lo, hi in spans if lo < match.start() < hi]
        if len(owner) != 1:
            counts['unmapped'] += 1
            continue
        filename, line = origins[text.count('\n', 0, match.start())]
        if filename and filename.startswith('/src/'):
            filename = filename[5:]
        if not filename or not filename.startswith(('src/', 'include/')):
            counts['nonproject_fixed_loops'] += 1
            continue
        counts['recognized_project_loops'] += 1
        found.append({'owner': owner[0], 'origin': filename, 'line': line,
                      'start': first, 'bound': bound, 'iterations': iterations,
                      'header': clean[match.start():end]})
    return found, counts


def indirect(path):
    text = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', str(path)], text=True)
    calls, current = Counter(), None
    for line in text.splitlines():
        heading = re.match(r'^[0-9a-f]+ <(.+)>:$', line)
        if heading:
            current = heading[1]
        if current and re.match(r'^\s*[0-9a-f]+:\s+call\s+\*', line):
            calls[current] += 1
    return calls


def controls():
    text = '# 41 "/src/include/dsplib/control.h"\nstatic void helper(void) { for (i = 0; i < (2 * (3 + 1)); ++i) {} }\n'
    found, _ = loops(text)
    assert len(found) == 1 and found[0]['iterations'] == 8
    assert (found[0]['origin'], found[0]['line']) == ('include/dsplib/control.h', 41)
    assert not loops(text.replace('(2 * (3 + 1))', 'n'))[0]
    assert not loops(text.replace('++i', 'i += 2'))[0]
    assert constant('(1 << 4)') == 16 and constant('sizeof(x)') is None
    assert not (4 >= 4 + 2)
    return {'equal_callback_negative': True, 'nested_header_provenance_positive': True, 'variable_bound_negative': True,
            'nonunit_step_negative': True, 'constant_expression_positive_refusal': True}


def main():
    root = d.ROOT
    pre = root / 'build/callback-preprocessed'
    provenance = json.loads((pre / 'provenance.json').read_text())
    assert provenance['exit'] == 0 and len(provenance['commands']) == 299
    census_path = root / 'build/byteident-residual-value-owner.json'
    census = json.loads(census_path.read_text())
    assert census['exact'] == 1079
    exact = set(census['exact_symbols'])
    assert all(hashlib.sha256((root / p).read_bytes()).hexdigest() == h for p,h in provenance['header_hashes'].items())
    assert provenance['config'] == (root/'build/tc_out/.build-config').read_text()
    original_sizes, original_edges = d.b.sizes(d.b.BLOB), backedges(d.b.BLOB)
    original_calls = indirect(d.b.BLOB)
    report = {'source_baseline': '80da7b0c', 'provenance_sha256': hashlib.sha256((pre/'provenance.json').read_bytes()).hexdigest(),
              'counts': Counter(), 'controls': controls(), 'nominations': [], 'loop_inventory': [], 'object_hashes': {}}
    prior = json.loads((root/'build/helper-caller-screen.json').read_text())
    old = {(r['source'], r['symbol']) for r in prior['nominations']}
    for relative, expected_hash in provenance['preprocessed_hashes'].items():
        source = root / relative
        assert hashlib.sha256(source.read_bytes()).hexdigest() == provenance['source_hashes'][relative]
        path = pre / (relative.replace('/', '_') + '.i')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected_hash
        fixed, counts = loops(path.read_text())
        report['counts'].update(counts)
        for loop in fixed:
            physical = (root / loop['origin']).read_text().splitlines()[loop['line']-1]
            literal = re.search(r'\b\w+\s*(?:<=|<)\s*\d+[uUlL]*\s*;', physical)
            loop['new_scope'] = loop['origin'].startswith('include/') or not literal or loop['bound'] > 32
        obj = root / 'build/tc_out' / (relative.replace('/', '_') + '.o')
        report['object_hashes'][obj.name] = hashlib.sha256(obj.read_bytes()).hexdigest()
        assert report['object_hashes'][obj.name] == prior['object_hashes'][obj.name]
        own_calls, own_edges, names = indirect(obj), backedges(obj), d.b.sizes(str(obj))
        demangled = subprocess.check_output(['c++filt'], input='\n'.join(names)+'\n', text=True).splitlines()
        owners = {n: dem.split('(', 1)[0] for n, dem in zip(names, demangled)}
        for loop in fixed:
            loop['possible_emitted_owners'] = [n for n,o in owners.items() if loop['owner'] in (o, o.rsplit('::',1)[-1])]
            report['counts']['loops_without_emitted_owner'] += not loop['possible_emitted_owners']
            report['counts']['loops_with_ambiguous_emitted_owner'] += len(loop['possible_emitted_owners']) > 1
            report['counts']['included_header_loops'] += loop['origin'].startswith('include/')
        report['loop_inventory'].append({'source': relative, 'loops': fixed, 'counts': dict(counts)})
        for name in names:
            report['counts']['emitted_bodies'] += 1
            if name not in original_sizes or name in exact:
                continue
            report['counts']['eligible_nonexact_bodies'] += 1
            matched = [loop for loop in fixed if loop['owner'] in (owners[name], owners[name].rsplit('::', 1)[-1])]
            if matched:
                report['counts']['fixed_loop_body_copies'] += 1
            missing = original_calls[name] >= own_calls[name] + 2
            novel_graph = any(l['new_scope'] for l in matched) and len(original_edges[name]) < len(own_edges[name])
            if not missing and not novel_graph:
                continue
            report['nominations'].append({'source': relative, 'symbol': name, 'owner': owners[name],
                'blob_bytes': original_sizes[name], 'ours_bytes': names[name],
                'blob_indirect_calls': original_calls[name], 'ours_indirect_calls': own_calls[name],
                'indirect_deficit': missing, 'new_loop_graph': bool(novel_graph),
                'previous_nominee': (relative, name) in old, 'loops': matched,
                'possible_emitted_owners': {l['owner']: [n for n,o in owners.items() if l['owner'] in (o, o.rsplit('::',1)[-1])] for l in matched},
                'blob_backedges': original_edges[name], 'ours_backedges': own_edges[name]})
        report['counts']['translation_units'] += 1
    put = next(r for r in report['nominations'] if r['symbol'] == 'putFrame')
    assert (put['blob_indirect_calls'], put['ours_indirect_calls']) == (18, 6)
    precoder = next(r for r in report['loop_inventory'] if r['source'] == 'src/pump/v90/V92Precoder.cpp')
    assert [l['iterations'] for l in precoder['loops'] if l['owner'] == 'V92Precoder::reset'] == [6, 12]
    name = '_ZN11V92Precoder5resetEP16V92MappingParams'
    assert len(original_edges[name]) == 0
    assert len(backedges(root/'build/tc_out/src_pump_v90_V92Precoder.cpp.o')[name]) == 2
    report['controls'].update(putFrame_live_positive=True, precoder_live_positive=True)
    assert report['counts']['translation_units'] == 299
    (root/'build/callback-loop-screen.json').write_text(json.dumps(report, indent=2) + '\n')
    print(dict(report['counts']))
    print(len(report['controls']), 'controls pass;', len(report['nominations']), 'nominations')
    for row in report['nominations']:
        print('prior' if row['previous_nominee'] else 'new', row['owner'], row['blob_bytes'],
              'callbacks', row['blob_indirect_calls'], row['ours_indirect_calls'],
              'loop graph', row['new_loop_graph'], [(l['origin'],l['line']) for l in row['loops']])


if __name__ == '__main__':
    main()
