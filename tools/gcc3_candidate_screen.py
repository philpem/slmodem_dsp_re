#!/usr/bin/env python3
"""Inventory constrained arithmetic and Playbook patterns without grading them.

Stack references are not labelled spills. Candidates need source/RTL review.
This reader never interprets relocation bytes as arithmetic operands.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b


def instructions(path):
    text = subprocess.check_output(['objdump', '-dr', '--no-show-raw-insn', str(path)], text=True)
    functions, current = {}, None
    for line in text.splitlines():
        match = re.match(r'^[0-9a-f]+ <(.+)>:$', line)
        if match:
            current = []
            functions[match[1]] = current
            continue
        match = re.match(r'^\s*[0-9a-f]+:\s+(\S+)\s*(.*)$', line)
        if current is not None and match and not match[1].startswith('R_'):
            current.append((match[1], match[2]))
    return functions


def features(rows):
    return {'divide': sum(m in ('idiv','idivl','idivw','div','divl','divw') for m,o in rows),
            'variable_shift': sum(m.startswith(('shl','shr','sar','sal')) and '%cl' in o for m,o in rows),
            'stack_references': sum('%esp' in o for m,o in rows),
            'word_tests': sum(m.startswith(('test','cmp')) and re.search(r'%(?:ax|bx|cx|dx)\b',o) is not None for m,o in rows),
            'sign_extensions': sum(m in ('cwtl','movswl') for m,o in rows),
            'unsigned_extensions': sum(m in ('movzwl','movzbl') for m,o in rows),
            'setcc': sum(m.startswith('set') for m,o in rows)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--object-dir', type=Path, default=ROOT/'build/tc_out')
    parser.add_argument('--baseline-report', type=Path, required=True)
    parser.add_argument('--excluded-paths', type=Path, required=True)
    parser.add_argument('--old-alpha-object', type=Path, required=True)
    parser.add_argument('--json-out', type=Path, required=True)
    args = parser.parse_args()
    exact = set(json.loads(args.baseline_report.read_text())['exact_symbols'])
    excluded = set(args.excluded_paths.read_text().splitlines())
    reference = instructions(b.BLOB)
    blob_sizes = b.sizes(b.BLOB)
    current = instructions(args.object_dir/'src_pump_v34_V34TX.c.o')
    historical = instructions(args.old_alpha_object)
    assert features(current['updateAlpha']) == features(reference['updateAlpha'])
    assert features(historical['updateAlpha'])['stack_references'] > features(reference['updateAlpha'])['stack_references']
    assert features(historical['updateAlpha'])['divide'] == features(reference['updateAlpha'])['divide'] == 1
    rows, missing = [], []
    compared = nonexact = 0
    manifest = (args.object_dir/'tc_manifest.txt').read_text().splitlines()
    allowed = ('src/pump/v34/', 'src/pump/v32/', 'src/pump/v22/', 'src/pump/v23/',
               'src/dsp/', 'src/callprog/', 'src/service/', 'src/call/')
    scoped = 0
    for line in manifest:
        name, source = line.split()
        if source in excluded or not source.startswith(allowed):
            continue
        scoped += 1
        path = args.object_dir/name
        candidate = instructions(path)
        for symbol, size in b.sizes(str(path)).items():
            if symbol not in blob_sizes:
                continue
            compared += 1
            if symbol in exact:
                continue
            nonexact += 1
            if symbol not in reference or symbol not in candidate:
                missing.append({'symbol':symbol,'source':source})
                continue
            left, right = features(reference[symbol]), features(candidate[symbol])
            families = []
            if left['divide'] or right['divide']:
                families.append('constrained-divide')
            if left['variable_shift'] or right['variable_shift']:
                families.append('shift-count')
            if left['word_tests'] != right['word_tests'] or left['sign_extensions'] != right['sign_extensions']:
                families.append('scalar-width')
            if families:
                rows.append({'symbol':symbol,'source':source,'object':name,
                             'blob_bytes':blob_sizes[symbol],'candidate_bytes':size,
                             'blob':left,'candidate':right,'families':families})
    rows.sort(key=lambda row:(not row['source'].startswith('src/pump/v34/'),
                              'constrained-divide' not in row['families'], row['blob_bytes']))
    result = {'provenance': {'revision': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'build_config': (args.object_dir/'.build-config').read_text(),
              'baseline_sha256': hashlib.sha256(args.baseline_report.read_bytes()).hexdigest(),
              'historical_control_sha256': hashlib.sha256(args.old_alpha_object.read_bytes()).hexdigest(),
              'object_sha256': {line.split()[0]:hashlib.sha256((args.object_dir/line.split()[0]).read_bytes()).hexdigest() for line in manifest}},
              'objects':len(manifest),'scoped_objects':scoped,'shared_symbols':compared,
              'nonexact_symbols':nonexact,'missing_disassembly':missing,
              'feature_candidates':len(rows),'controls':{'exact_alpha':True,'historical_stack_delta':True,'divide_count':True},
              'excluded_paths':sorted(excluded),'rows':rows}
    args.json_out.parent.mkdir(parents=True,exist_ok=True)
    args.json_out.write_text(json.dumps(result,indent=2)+'\n')
    print(f"{len(manifest)} objects; {scoped} in scope; {compared} shared symbols; {nonexact} nonexact; {len(missing)} missing-disassembly refusals; {len(rows)} feature candidates")
    print('3/3 known arithmetic/stack/exact controls pass; feature matches are not source preimages')
    for row in rows:
        if 'constrained-divide' in row['families']:
            print(row['symbol'],row['blob_bytes'],row['candidate_bytes'],row['blob']['stack_references'],row['candidate']['stack_references'],row['source'])


if __name__ == '__main__':
    main()
