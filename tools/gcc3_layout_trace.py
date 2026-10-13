#!/usr/bin/env python3
"""Extract retained prediction, trace ordering and x87 edge repair evidence."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
# The local dis.py must not shadow stdlib exception reporting.
_original_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
sys.path[:] = _original_path
from gcc3_stage_divergence import chunks


def extract(folder, stem, header):
    stages = {}
    for stage in ('12.bp','31.bbro','34.stack'):
        path = folder / (stem + '.' + stage)
        selected = chunks(path,header)
        if len(selected) != 1:
            raise ValueError('ambiguous function header; explicit clone support required')
        stages[stage] = {'text':selected[0], 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    predictions, current = [], None
    for line in stages['12.bp']['text'].splitlines():
        match = re.fullmatch(r'Predictions for insn (\d+) bb (\d+)',line)
        if match:
            current = {'uid':int(match[1]),'block':int(match[2]),'heuristics':[]}
            predictions.append(current)
        else:
            match = re.fullmatch(r'  (.+heuristics(?: \(ignored\))?): ([0-9.]+)%',line)
            if match and current is not None:
                current['heuristics'].append({'name':match[1],'percent':float(match[2])})
    if not predictions or any(not p['heuristics'] for p in predictions):
        raise ValueError('missing prediction observations')
    text = stages['31.bbro']['text']
    traces = []
    for number,roundno,line in re.findall(r'^Trace (\d+) \(round (\d+)\):\s*(.*)$',text,re.M):
        blocks = [{'block':int(b),'frequency':int(f)} for b,f in re.findall(r'(\d+) \[(\d+)\]',line)]
        if not blocks:
            raise ValueError('empty reorder trace')
        traces.append({'trace':int(number),'round':int(roundno),'blocks':blocks})
    orders = re.findall(r'^Final order:\n([0-9 ]+)\n',text,re.M)
    if len(orders) != 1 or not traces:
        raise ValueError('missing or ambiguous final block order')
    order = [int(n) for n in orders[0].split()]
    flattened = [b['block'] for t in traces for b in t['blocks']]
    copies = [{'original':int(a),'copy':int(b)} for a,b in re.findall(r'^Duplicated bb (\d+) \(created bb (\d+)\)',text,re.M)]
    for copy in copies:
        if copy['original'] not in flattened:
            raise ValueError('copy source not observed in traces')
    covered = set(flattened) | {c['copy'] for c in copies}
    if len(set(order)) != len(order) or set(order) != covered:
        raise ValueError('trace/final block coverage disagreement')
    repairs = [{'from':int(a),'to':int(b),'action':action} for a,b,action in
               re.findall(r'^Edge (-?\d+)->(-?\d+): (.+)$',stages['34.stack']['text'],re.M)]
    if not repairs:
        raise ValueError('no x87 edge observations')
    return {'folder':str(folder),'header':header,'dump_hashes':{s:x['sha256'] for s,x in stages.items()},
            'predictions':predictions,'reorder_traces':traces,'final_order':order,'copied_blocks':copies,
            'x87_edges':repairs,'denominator':{'predicted_branches':len(predictions),
            'reorder_traces':len(traces),'ordered_blocks':len(order),'x87_edges':len(repairs)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dump-dir',required=True,type=Path)
    parser.add_argument('--stem',required=True)
    parser.add_argument('--header',required=True)
    parser.add_argument('--out',required=True,type=Path)
    args = parser.parse_args()
    report = extract(args.dump_dir,args.stem,args.header)
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print('layout trace:',report['denominator'],'final order',report['final_order'])


if __name__ == '__main__':
    main()
