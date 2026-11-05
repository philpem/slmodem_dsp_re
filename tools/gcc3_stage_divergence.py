#!/usr/bin/env python3
"""Compare existing GCC3 RTL streams and observable immediate-store splits.

No compilation, register erasure, dynamic scratch-state reconstruction or
byte-exact grading is performed.
"""
import argparse
import hashlib
from collections import Counter
from difflib import SequenceMatcher
import json
import re
from pathlib import Path
from gcc3_reload_trace import instructions, register

NONSTREAM = {'00.cgraph', '08.gcse'}


def cgraph_summary(text):
    """Extract printed GCC3 callgraph records without inventing ABI clone IDs."""
    phases, current, record = {}, None, None
    decisions = []
    unparsed = []
    for number, line in enumerate(text.splitlines(), 1):
        heading = re.fullmatch(r'([A-Za-z ]+ callgraph):', line)
        function = re.fullmatch(r'(.+): ([0-9]+) insns(.*)', line)
        if heading:
            current = heading[1]
            phases.setdefault(current, [])
            record = None
        elif function and current is not None:
            record = {'printed_name': function[1], 'tree_insns': int(function[2]),
                      'attributes': function[3].strip(), 'edges': {}}
            phases[current].append(record)
        elif record is not None and re.match(r'^  (called by|calls):', line):
            key, value = line.strip().split(':', 1)
            record['edges'][key] = value.strip()
        elif line and not line[0].isspace():
            record = None
            if any(word in line.lower() for word in ('inlin', 'growth', 'reclaim')):
                decisions.append({'line': number, 'text': line})
            else:
                unparsed.append({'line': number, 'text': line})
        elif line.strip():
            unparsed.append({'line': number, 'text': line})
    return {'phases': phases, 'function_records': sum(map(len, phases.values())),
            'decision_lines': decisions, 'unparsed_lines': unparsed,
            'scope': 'whole-TU printed names and ordered records; ABI clone and RTL correspondence unproven'}


def normalize(node):
    """Normalize only explicit GCC tree-pointer annotations, never immediates."""
    if not isinstance(node, list):
        return node
    output = [normalize(child) for child in node]
    for index in range(1, len(output)):
        if (isinstance(output[index-1], str)
                and re.fullmatch(r'<[a-z_]+(?:_decl|_type|_cst)', output[index-1])
                and isinstance(output[index], str)
                and re.fullmatch(r'0x[0-9a-f]+>?', output[index])):
            output[index] = '<tree-address>' + ('>' if output[index].endswith('>') else '')
    return output


def discover(folder, stem=None):
    found = {}
    for path in folder.iterdir():
        match = re.fullmatch(r'(.+)\.([0-9]{2}\.[a-z0-9_]+)', path.name)
        if path.is_file() and match and (stem is None or match[1] == stem):
            found.setdefault(match[1], {})[match[2]] = path
    if len(found) != 1:
        raise ValueError('require one dump stem; use --stem: ' + str(sorted(found)))
    prefix, stages = next(iter(found.items()))
    excluded = sorted(set(stages) & NONSTREAM)
    stages = {stage: path for stage, path in stages.items() if stage not in NONSTREAM}
    if not stages:
        raise ValueError('no instruction-stream stages')
    return prefix, stages, excluded


def chunks(path, header, content=None):
    text = path.read_bytes().decode() if content is None else content.decode()
    blocks = re.split(r'^;; Function ', text, flags=re.M)[1:]
    selected = [block for block in blocks if block.splitlines()[0].strip() == header]
    if not selected:
        available = [block.splitlines()[0].strip() for block in blocks]
        raise ValueError('function header absent in %s: %s; available: %s'
                         % (path, header, available))
    return selected


def selected_streams(path, header, clone=None, all_clones=False, content=None):
    blocks = chunks(path, header, content)
    if all_clones:
        indexes = list(range(len(blocks)))
    elif clone is not None:
        if not 0 <= clone < len(blocks):
            raise ValueError('clone index outside matched headers')
        indexes = [clone]
    elif len(blocks) == 1:
        indexes = [0]
    else:
        raise ValueError('ambiguous function header; use --clone or --all-clones')
    return len(blocks), {index: instructions(blocks[index]) for index in indexes}


def sets(node):
    if not isinstance(node, list):
        return []
    if node and node[0] == 'set':
        return [node]
    return [found for child in node for found in sets(child)]


def fingerprint(node):
    return json.dumps(normalize(node), separators=(',', ':'))


def shape(node):
    """Alignment key only. Reports always retain complete operand evidence."""
    if not isinstance(node, list):
        return '<atom>'
    return [node[0]] + [shape(child) for child in node[1:]] if node else []


def evidence(node):
    regs, annotations = [], []
    def visit(item):
        if not isinstance(item, list):
            return
        reg = register(item)
        if reg is not None:
            regs.append({'number': reg, 'kind': 'pseudo' if reg >= 53 else 'hard',
                         'operand': normalize(item)})
        if any(isinstance(x, str) and ('[' in x or '<' in x) for x in item):
            annotations.append(normalize(item))
        for child in item:
            visit(child)
    visit(node)
    return {'register_operands': regs, 'retained_annotations': annotations}


def operand_changes(a, b, path=()):
    if a == b:
        return []
    if isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        return [change for i, (x, y) in enumerate(zip(a, b))
                for change in operand_changes(x, y, path + (i,))]
    return [{'path': list(path), 'left': normalize(a), 'right': normalize(b)}]


def structural_alignment(a, b, limit):
    """Unique monotone exact anchors, then unique shapes inside each gap.

    Never guess an association of repeated shapes, even if position suggests
    one. No pseudo, label, constant, mode or subreg evidence is normalized away.
    """
    au, bu = list(a), list(b)
    ak, bk = [fingerprint(a[u]) for u in au], [fingerprint(b[u]) for u in bu]
    ac, bc = Counter(ak), Counter(bk)
    if ak == bk:
        return {'status': 'complete', 'equal': True,
                'aligned_pairs': len(au), 'changed_pair_count': 0,
                'uid_renumbered_pairs': sum(x != y for x, y in zip(au, bu)),
                'unmatched_left_uids': [], 'unmatched_right_uids': [],
                'ambiguities': [], 'examples': [],
                'scope': 'identical ordered patterns; UIDs excluded, operands retained'}
    anchors = []
    for block in SequenceMatcher(None, ak, bk, autojunk=False).get_matching_blocks():
        for i in range(block.size):
            x, y = block.a + i, block.b + i
            if ac[ak[x]] == bc[bk[y]] == 1:
                anchors.append((x, y))
    pairs, ambiguous = [], []
    last_a = last_b = -1
    for x, y in anchors + [(len(au), len(bu))]:
        aa, bb = list(range(last_a + 1, x)), list(range(last_b + 1, y))
        ash = [json.dumps(shape(a[au[i]])) for i in aa]
        bsh = [json.dumps(shape(b[bu[i]])) for i in bb]
        ca, cb = Counter(ash), Counter(bsh)
        proposed = [(i, bb[bsh.index(key)]) for i, key in zip(aa, ash)
                    if ca[key] == cb[key] == 1]
        # Crossing matches indicate reordering; preserve as unresolved evidence.
        if [j for _, j in proposed] == sorted(j for _, j in proposed):
            pairs.extend((i, j, 'unique-shape-in-gap') for i, j in proposed)
        elif proposed:
            ambiguous.append({'reason': 'crossing shape candidates',
                              'left_uids': [au[i] for i in aa],
                              'right_uids': [bu[j] for j in bb]})
        repeated = [key for key in ca.keys() & cb.keys() if ca[key] > 1 or cb[key] > 1]
        if repeated:
            ambiguous.append({'reason': 'repeated shapes in unmatched gap',
                              'left_uids': [au[i] for i in aa],
                              'right_uids': [bu[j] for j in bb]})
        if x < len(au):
            pairs.append((x, y, 'unique-exact-anchor'))
        last_a, last_b = x, y
    pairs.sort()
    aligned_a, aligned_b = {i for i, _, _ in pairs}, {j for _, j, _ in pairs}
    changed = [(i, j, why) for i, j, why in pairs if ak[i] != bk[j]]
    unmatched_a = [au[i] for i in range(len(au)) if i not in aligned_a]
    unmatched_b = [bu[j] for j in range(len(bu)) if j not in aligned_b]
    complete = not unmatched_a and not unmatched_b and not ambiguous
    return {'status': 'complete' if complete else 'ambiguous-or-unmatched',
            'equal': complete and not changed,
            'aligned_pairs': len(pairs), 'changed_pair_count': len(changed),
            'uid_renumbered_pairs': sum(au[i] != bu[j] for i, j, _ in pairs),
            'unmatched_left_uids': unmatched_a, 'unmatched_right_uids': unmatched_b,
            'ambiguities': ambiguous,
            'unmatched_examples': {
                'left': [{'uid': u, 'pattern': normalize(a[u]), 'evidence': evidence(a[u])} for u in unmatched_a[:limit]],
                'right': [{'uid': u, 'pattern': normalize(b[u]), 'evidence': evidence(b[u])} for u in unmatched_b[:limit]]},
            'examples': [{'left_uid': au[i], 'right_uid': bu[j], 'basis': why,
                          'left': normalize(a[au[i]]), 'right': normalize(b[bu[j]]),
                          'left_evidence': evidence(a[au[i]]),
                          'right_evidence': evidence(b[bu[j]]),
                          'operand_changes': operand_changes(normalize(a[au[i]]), normalize(b[bu[j]]))}
                         for i, j, why in changed[:limit]],
            'scope': 'instruction patterns only; label refs remain literal; notes/CFG not aligned'}


def immediate(node):
    return (isinstance(node, list) and node
            and node[0] in ('const_int', 'const_double:SF', 'const_double:DF',
                           'const_double:XF'))


def store_splits(before, after):
    """Recognize observable immediate->register->same-memory transformations.

    A recognizable split is not a count of scratch searches, successful
    allocations, rejected candidates or cursor transitions.
    """
    old = {}
    new = {}
    assignments = [(uid, assignment) for uid, pattern in after.items()
                   for assignment in sets(pattern)]
    for uid, pattern in before.items():
        for assignment in sets(pattern):
            dest, value = assignment[1:3]
            if isinstance(dest, list) and str(dest[0]).startswith('mem'):
                old.setdefault(fingerprint(dest), []).append((uid, dest, value))
    for index, (uid, assignment) in enumerate(assignments):
        dest, value = assignment[1:3]
        if isinstance(dest, list) and str(dest[0]).startswith('mem'):
            new.setdefault(fingerprint(dest), []).append((index, uid, value))
    found = []
    for destination in sorted(set(old) & set(new)):
        if len(old[destination]) != 1 or len(new[destination]) != 1:
            continue
        old_uid, dest, old_value = old[destination][0]
        index, new_uid, new_value = new[destination][0]
        reg = register(new_value)
        if not immediate(old_value) or reg is None:
            continue
        defining = next(((uid, assignment) for uid, assignment
                         in reversed(assignments[:index])
                         if register(assignment[1]) == reg), None)
        if defining is None or fingerprint(defining[1][2]) != fingerprint(old_value):
            continue
        found.append({'before_store_uid': old_uid, 'after_store_uid': new_uid,
                      'materializer_uid': defining[0], 'register': reg,
                      'destination': normalize(dest), 'value': normalize(old_value)})
    return found


def compare(left, right, header, stem=None, clone=None, all_clones=False,
            limit=8, scratch_before='27.flow2', scratch_after='28.peephole2'):
    lp, ls, le = discover(left, stem)
    rp, rs, re_ = discover(right, stem)
    if set(ls) != set(rs):
        raise ValueError('stage sets differ; cannot claim earliest divergence')
    stages = []
    stored = {'left': {}, 'right': {}}
    match_counts = None
    stream_count = 0
    for stage in sorted(ls):
        # Parse and hash the same captured bytes, even if a producer rewrites
        # the pathname during analysis. This is per-file, not a build snapshot.
        left_bytes, right_bytes = ls[stage].read_bytes(), rs[stage].read_bytes()
        lc, lrows = selected_streams(ls[stage], header, clone, all_clones, left_bytes)
        rc, rrows = selected_streams(rs[stage], header, clone, all_clones, right_bytes)
        if lc != rc or set(lrows) != set(rrows):
            raise ValueError('clone sets differ')
        if match_counts is None:
            match_counts = [lc, rc]
        if match_counts != [lc, rc]:
            raise ValueError('matched clone count changes between stages')
        if stage in (scratch_before, scratch_after):
            stored['left'][stage] = lrows
            stored['right'][stage] = rrows
        row = {'stage': stage, 'left_sha256': hashlib.sha256(left_bytes).hexdigest(),
               'right_sha256': hashlib.sha256(right_bytes).hexdigest(),
               'clones': []}
        for index in lrows:
            a, b = lrows[index], rrows[index]
            stream_count += 2
            changed = [uid for uid in sorted(set(a) | set(b))
                       if uid not in a or uid not in b
                       or fingerprint(a[uid]) != fingerprint(b[uid])]
            order_equal = list(a) == list(b)
            row['clones'].append({'index': index, 'left_instructions': len(a),
                                  'right_instructions': len(b),
                                  'equal': not changed and order_equal,
                                  'structural_alignment': structural_alignment(a, b, limit),
                                  'instruction_uid_order_equal': order_equal,
                                  'changed_uid_count': len(changed),
                                  'changed_uids': changed,
                                  'examples': [{'uid': uid,
                                                'left': normalize(a.get(uid)),
                                                'right': normalize(b.get(uid))}
                                               for uid in changed[:limit]]})
        stages.append(row)
    first = {str(index): next((row['stage'] for row in stages
                               if not next(c for c in row['clones']
                                           if c['index'] == index)['equal']), None)
             for index in lrows}
    structural_first = {str(index): next((row['stage'] for row in stages
        if not next(c for c in row['clones'] if c['index'] == index)['structural_alignment']['equal']), None)
        for index in lrows}
    nonstream = []
    for stage in sorted(set(le) | set(re_)):
        apath, bpath = left / (lp + '.' + stage), right / (rp + '.' + stage)
        left_bytes = apath.read_bytes() if apath.exists() else None
        right_bytes = bpath.read_bytes() if bpath.exists() else None
        nonstream.append({'stage': stage, 'scope': 'whole dump bytes, no RTL/causal claim',
            'status': 'raw-equal' if left_bytes is not None and right_bytes is not None and left_bytes == right_bytes else 'different-or-missing; unsupported structural format',
            'left_sha256': hashlib.sha256(left_bytes).hexdigest() if left_bytes is not None else None,
            'right_sha256': hashlib.sha256(right_bytes).hexdigest() if right_bytes is not None else None})
        if stage == '00.cgraph' and left_bytes is not None and right_bytes is not None:
            left_graph, right_graph = cgraph_summary(left_bytes.decode()), cgraph_summary(right_bytes.decode())
            nonstream[-1]['callgraph'] = {'left': left_graph, 'right': right_graph,
                'printed_records_equal': left_graph['phases'] == right_graph['phases'],
                'nonempty_both': bool(left_graph['function_records'] and right_graph['function_records'])}
    scratch = {}
    for label in ('left', 'right'):
        source = stored[label]
        if scratch_before in source and scratch_after in source:
            scratch[label] = {str(index): store_splits(source[scratch_before][index],
                                                      source[scratch_after][index])
                              for index in lrows}
    return {'function_header': header, 'dump_stems': [lp, rp],
            'directories': [str(left.resolve()), str(right.resolve())],
            'matched_clones_per_directory': match_counts,
            'selected_clones': list(lrows), 'instruction_streams': stream_count,
            'stage_pairs': len(stages), 'excluded_nonstream_dumps': [le, re_],
            'first_pattern_or_order_divergence': first,
            'first_observed_structural_difference_or_unresolved': structural_first,
            'interpretation': 'earliest observed dump difference is not allocator causality; original blob RTL unavailable',
            'nonstream_comparisons': nonstream, 'stages': stages,
            'store_split_window': [scratch_before, scratch_after],
            'observable_store_splits': scratch,
            'store_split_scope': 'unique destination / nearest preceding SET; no CFG or liveness proof',
            'dynamic_scratch_searches_or_cursor_values': 'not measured'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('left', type=Path)
    ap.add_argument('right', type=Path)
    ap.add_argument('--function', required=True, help='exact dump function header')
    ap.add_argument('--stem', help='dump filename prefix, e.g. V90Parameters.cpp')
    selection = ap.add_mutually_exclusive_group()
    selection.add_argument('--clone', type=int, help='zero-based matched header index')
    selection.add_argument('--all-clones', action='store_true')
    ap.add_argument('--max-examples', type=int, default=8)
    ap.add_argument('--scratch-before', default='27.flow2')
    ap.add_argument('--scratch-after', default='28.peephole2')
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if args.max_examples < 0:
        ap.error('--max-examples must be nonnegative')
    try:
        result = compare(args.left, args.right, args.function, args.stem,
                         args.clone, args.all_clones, args.max_examples,
                         args.scratch_before, args.scratch_after)
    except (ValueError, OSError) as error:
        ap.error(str(error))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('%d stage pairs / %d RTL streams / selected clones %s'
          % (result['stage_pairs'], result['instruction_streams'],
             result['selected_clones']))
    print('first UID/pattern divergence: ' + json.dumps(result['first_pattern_or_order_divergence']))
    print('first observed structural difference/unresolved: ' + json.dumps(result['first_observed_structural_difference_or_unresolved']))
    print('Earliest observed dump difference is not allocator causality.')
    print('observable store splits: ' + json.dumps(
        {side: {index: len(items) for index, items in clones.items()}
         for side, clones in result['observable_store_splits'].items()}))
    print('dynamic scratch searches/cursor values: not measured')


if __name__ == '__main__':
    main()
