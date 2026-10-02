#!/usr/bin/env python3
"""Replay six known DetSequence loop/lifetime controls from saved compiler dumps."""
import argparse
import hashlib
import json
import re
import playbook_small_patterns as driver


def analyze(root, domain_name):
    directory = root / 'build' / domain_name
    result = json.loads((directory / 'results.json').read_text())
    cells = result['families']['V32prc']['cells']
    expected = ({'baseline': (False, False, False),
                 'countdown': (True, False, False),
                 'shift-counter': (False, True, False),
                 'both': (True, True, False)} if domain_name == 'playbook-detsequence'
                else {'baseline': (True, True, False), 'found-once': (True, True, True)})
    assert set(cells) == set(expected), 'incomplete domain'
    analysis = {'domain': result['domain'], 'revision': result['revision'],
                'control_count': len(expected), 'controls_passed': 0, 'cells': {}}
    for name, wanted in expected.items():
        path = directory / 'V32prc' / name
        cell = cells[name]
        assert hashlib.sha256((path / 'candidate.o').read_bytes()).hexdigest() == cell['object_hash']
        stages = {}
        for stage in ('01.rtl', '24.lreg', '25.greg'):
            rtl = (path / ('V32prc.c.' + stage)).read_text()
            stages[stage] = rtl.split(';; Function DetSequence')[1].split(';; Function')[0]
            (path / ('DetSequence-' + stage)).write_text(stages[stage])
        initial = stages['01.rtl']
        found = re.search(r'\(set \(reg/v:SI \d+ \[ found \]\)\s*\(const_int 0 ', initial)
        assert found, 'missing found initialization'
        countdown = bool(re.search(r'\(plus:SI \(reg/v:SI \d+ \[ count \]\)\s*\(const_int -1 ', initial))
        shift = bool(re.search(r'\[ shift \]', initial))
        found_once = found.start() < initial.index('NOTE_INSN_LOOP_BEG')
        actual = (countdown, shift, found_once)
        assert actual == wanted, 'known RTL graph control failed: ' + name
        assert not re.search(r'reload_(?:in|out).*nbits', stages['24.lreg'])
        bound_spilled = bool(re.search(r'Reload \d+: reload_(?:in|out) \(SI\) = \(reg/v:SI 64 \[ nbits \]\)', stages['25.greg']))
        assert bound_spilled == shift, 'known bound spill control failed: ' + name
        priority = re.search(r';; \d+ regs to allocate:([^\n]*)', stages['25.greg'])
        costs = {}
        for label, regno in [('nbits', 64), ('reg', 66)]:
            matches = re.findall(r'Register ' + str(regno) + r' costs:[^\n]*\bMEM:([0-9]+)', stages['24.lreg'])
            costs[label] = [int(n) for n in matches]
        analysis['controls_passed'] += 1
        analysis['cells'][name] = {
            'countdown': countdown, 'shift_counter': shift, 'found_once': found_once,
            'nbits_spilled_in_greg': bound_spilled,
            'allocation_priority': priority.group(1).split(), 'memory_costs': costs,
            'functions': len(cell['functions']), 'globals': len(cell['globals']),
            'verdict': cell['verdicts']['DetSequence'],
            'changed_bodies': cell.get('changed_bodies', []),
            'gains': cell.get('gains', []), 'losses': cell.get('losses', [])}
    (directory / 'analysis.json').write_text(json.dumps(analysis, indent=2) + '\n')
    print(domain_name, 'RTL/spill controls:', analysis['controls_passed'], '/', analysis['control_count'])
    return analysis


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loop-domain', required=True)
    parser.add_argument('--found-domain', required=True)
    args = parser.parse_args()
    for directory, domain in [('playbook-detsequence', args.loop_domain),
                              ('playbook-detsequence-found', args.found_domain)]:
        saved = json.loads((driver.ROOT / 'build' / directory / 'results.json').read_text())
        assert saved['domain'] == domain, 'declared domain drift'
        analyze(driver.ROOT, directory)
