#!/usr/bin/env python3
"""Compare existing GCC3 RTL streams and observable immediate-store splits.

No compilation, register erasure, dynamic scratch-state reconstruction or
byte-exact grading is performed.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from gcc3_reload_trace import instructions, register

NONSTREAM = {'00.cgraph', '08.gcse'}


def normalize(node):
    """Normalize only explicit GCC tree-pointer annotations, never immediates."""
    if not isinstance(node, list):
        return node
    output = [normalize(child) for child in node]
    for index in range(1, len(output)):
        if (isinstance(output[index-1], str)
                and re.fullmatch(r'<[a-z_]+(?:_decl|_type)', output[index-1])
                and isinstance(output[index], str)
                and re.fullmatch(r'0x[0-9a-f]+', output[index])):
            output[index] = '<tree-address>'
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


def chunks(path, header):
    blocks = re.split(r'^;; Function ', path.read_text(), flags=re.M)[1:]
    selected = [block for block in blocks if block.splitlines()[0].strip() == header]
    if not selected:
        available = [block.splitlines()[0].strip() for block in blocks]
        raise ValueError('function header absent in %s: %s; available: %s'
                         % (path, header, available))
    return selected


def selected_streams(path, header, clone=None, all_clones=False):
    blocks = chunks(path, header)
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
        lc, lrows = selected_streams(ls[stage], header, clone, all_clones)
        rc, rrows = selected_streams(rs[stage], header, clone, all_clones)
        if lc != rc or set(lrows) != set(rrows):
            raise ValueError('clone sets differ')
        if match_counts is None:
            match_counts = [lc, rc]
        if match_counts != [lc, rc]:
            raise ValueError('matched clone count changes between stages')
        if stage in (scratch_before, scratch_after):
            stored['left'][stage] = lrows
            stored['right'][stage] = rrows
        row = {'stage': stage, 'left_sha256': hashlib.sha256(ls[stage].read_bytes()).hexdigest(),
               'right_sha256': hashlib.sha256(rs[stage].read_bytes()).hexdigest(),
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
            'first_pattern_or_order_divergence': first, 'stages': stages,
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
    print('first divergence: ' + json.dumps(result['first_pattern_or_order_divergence']))
    print('observable store splits: ' + json.dumps(
        {side: {index: len(items) for index, items in clones.items()}
         for side, clones in result['observable_store_splits'].items()}))
    print('dynamic scratch searches/cursor values: not measured')


if __name__ == '__main__':
    main()
