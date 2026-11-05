#!/usr/bin/env python3
"""Immutable, interaction-aware source recovery evidence ledger.

Manifest v1: id, hypothesis, symbol, tu, native_witness, compiler (identity,
flags), inputs [{path,sha256}], components {A: description,B: description},
fixture_domain, negative_controls, prior_attempts, reopen_rationale, reference,
cells {baseline,A,B,AB: {components: [], object: path, commands: [{argv: [],
cwd: path, outputs: [path]}], evidence: {period|linked_body: {command: index,
result: path}}}}. Evidence result JSON must contain passed and total integer
counts; only a successful associated command and positive total can pass.
All paths are relative to the invocation cwd, or absolute. Use frozen artifacts
as inputs; commands must write new outputs, never overwrite existing evidence.
Commands are intentionally arbitrary explicit argv, with shell=False. Importing
historical objects is supported but never establishes compilation provenance.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime, timezone


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_new(path, value):
    with Path(path).open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')


def freshness(inputs):
    errors = []
    for item in inputs:
        path = Path(item['path'])
        if not path.is_file():
            errors.append('missing: ' + str(path))
        elif digest(path) != item['sha256']:
            errors.append('stale: ' + str(path))
    return errors


def absolute_manifest(manifest):
    """Pin invocation-relative paths so later queries work from another cwd."""
    m = copy.deepcopy(manifest)
    def absolute(path):
        return str(Path(path).resolve())
    for item in m['inputs']:
        item['path'] = absolute(item['path'])
    m['reference'] = absolute(m['reference'])
    if m.get('retrieved_evidence_packet'):
        m['retrieved_evidence_packet'] = absolute(m['retrieved_evidence_packet'])
    for cell in m['cells'].values():
        cell['object'] = absolute(cell['object'])
        for command in cell.get('commands', []):
            command['cwd'] = absolute(command.get('cwd', '.'))
            command['outputs'] = [absolute(p) for p in command.get('outputs', [])]
        for spec in cell.get('evidence', {}).values():
            if 'result' in spec:
                spec['result'] = absolute(spec['result'])
    return m


def validate(m):
    if m.get('schema') != 'recovery-experiment-v1':
        raise ValueError('expected schema recovery-experiment-v1')
    for key in ('id', 'hypothesis', 'symbol', 'tu', 'native_witness',
                'compiler', 'inputs', 'components', 'fixture_domain',
                'negative_controls', 'reference'):
        if not m.get(key):
            raise ValueError('missing/empty ' + key)
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', m['id']) or m['id'] in ('.', '..'):
        raise ValueError('invalid id')
    if not m['compiler'].get('identity') or not isinstance(m['compiler'].get('flags'), list):
        raise ValueError('compiler identity and explicit flags required')
    if 'prior_attempts' not in m or 'reopen_rationale' not in m:
        raise ValueError('prior_attempts and reopen_rationale required (may be empty)')
    if m['prior_attempts'] and not m['reopen_rationale']:
        raise ValueError('reopened attempt requires rationale')
    expected = {'baseline': [], 'A': ['A'], 'B': ['B'], 'AB': ['A', 'B']}
    if set(m['components']) != {'A', 'B'} or set(m.get('cells', {})) != set(expected):
        raise ValueError('explicit baseline/A/B/AB and two component descriptions required')
    paths = {str(Path(i['path']).resolve()) for i in m['inputs']}
    if str(Path(m['reference']).resolve()) not in paths:
        raise ValueError('reference must be a hashed input')
    produced = set()
    packet = m.get('retrieved_evidence_packet')
    if packet:
        if str(Path(packet).resolve()) not in paths:
            raise ValueError('retrieved evidence packet must be a hashed input')
        frozen = json.loads(Path(packet).read_text())
        if frozen.get('schema') != 'recovery-memory-packet-v1' or not frozen.get('records'):
            raise ValueError('invalid/empty frozen retrieved evidence packet')
    for name, components in expected.items():
        cell = m['cells'][name]
        if sorted(cell.get('components', [])) != components or not cell.get('object'):
            raise ValueError('incorrect components/object: ' + name)
        commands = cell.get('commands', [])
        for command in commands:
            argv = command.get('argv')
            if not isinstance(argv, list) or not argv or not all(isinstance(a, str) for a in argv):
                raise ValueError('command requires nonempty string argv')
            for output in command.get('outputs', []):
                resolved = str(Path(output).resolve())
                if resolved in paths or resolved in produced or Path(output).exists():
                    raise ValueError('output already exists or overlaps inputs: ' + output)
                produced.add(resolved)
        if str(Path(cell['object']).resolve()) not in paths | produced:
            raise ValueError('cell object must be hashed input or declared output')
    return freshness(m['inputs'])


def evidence_state(spec, commands):
    if spec is None:
        return {'status': 'not-run', 'total': 0}
    index = spec.get('command')
    if not isinstance(index, int) or index < 0 or index >= len(commands):
        return {'status': 'unknown', 'total': 0, 'reason': 'no associated command'}
    command = commands[index]
    if command['status'] != 'passed':
        return {'status': 'failed', 'total': 0, 'reason': 'associated command failed'}
    try:
        path = Path(spec['result'])
        if str(path.resolve()) not in {str(Path(p).resolve()) for p in command['outputs']}:
            raise ValueError('result not a declared command output')
        result = json.loads(path.read_text())
        total, passed = result['total'], result['passed']
        if type(total) is not int or type(passed) is not int or total <= 0 or not 0 <= passed <= total:
            raise ValueError('invalid/empty denominator')
        return {'status': 'passed' if passed == total else 'failed', 'passed': passed,
                'total': total, 'result': str(path.resolve()), 'sha256': digest(path)}
    except (OSError, KeyError, ValueError, TypeError) as exc:
        return {'status': 'unknown', 'total': 0, 'reason': str(exc)}


def run(m, ledger, session=None):
    m = absolute_manifest(m)
    errors = validate(m)
    if errors:
        raise ValueError('; '.join(errors))
    record = Path(ledger) / m['id']
    record.mkdir(parents=True, exist_ok=False)
    write_new(record / 'manifest.json', m)
    result = {'schema': 'recovery-record-v1', 'id': m['id'],
              'created': datetime.now(timezone.utc).isoformat(), 'cells': {},
              'manifest_sha256': digest(record / 'manifest.json'),
              'checker': {'path': str(Path(__file__).resolve()), 'sha256': digest(__file__)},
              'scope': 'Measurements only; command success and object grades do not prove source fidelity.'}
    if session is None:
        from gcc3_search_compare import ComparisonSession
        session = ComparisonSession(m['reference'])
    result['analysis_inputs'] = [
        {'path': str(Path(path).resolve()), 'sha256': digest(path)}
        for path in getattr(session, '_TOOL_INPUTS', ())]
    generated = []
    for name in ('baseline', 'A', 'B', 'AB'):
        cell = m['cells'][name]
        commands = []
        blocked = False
        for index, command in enumerate(cell.get('commands', [])):
            entry = {'argv': command['argv'], 'cwd': str(Path(command.get('cwd', '.')).resolve()),
                     'outputs': {}, 'status': 'not-run', 'exit': None}
            if not blocked:
                try:
                    changed = freshness(m['inputs'] + generated)
                    if changed:
                        raise OSError('evidence changed before command: ' + '; '.join(changed))
                    if any(Path(p).exists() for p in command.get('outputs', [])):
                        raise OSError('refusing to overwrite command output')
                    proc = subprocess.run(command['argv'], cwd=entry['cwd'], shell=False,
                                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    output, entry['exit'] = proc.stdout, proc.returncode
                except OSError as exc:
                    output = str(exc).encode()
                log = record / ('%s-%d.log' % (name, index))
                with log.open('xb') as stream:
                    stream.write(output)
                entry.update(log=str(log.resolve()), log_sha256=digest(log))
                entry['outputs'] = {p: digest(p) for p in command.get('outputs', []) if Path(p).is_file()}
                generated.extend({'path': p, 'sha256': sha} for p, sha in entry['outputs'].items())
                entry['status'] = ('passed' if entry['exit'] == 0 and
                                   len(entry['outputs']) == len(command.get('outputs', [])) else 'failed')
                blocked = entry['status'] != 'passed'
            commands.append(entry)
        row = {'components': cell['components'], 'commands': commands,
               'period': evidence_state(cell.get('evidence', {}).get('period'), commands),
               'linked_body': evidence_state(cell.get('evidence', {}).get('linked_body'), commands)}
        baseline_failed = name != 'baseline' and any(c['status'] != 'passed' for c in result['cells']['baseline']['commands'])
        if blocked or baseline_failed or freshness(m['inputs'] + generated):
            row['comparison'] = {'status': 'not-run', 'reason': 'failed command or stale inputs'}
        else:
            try:
                comparison = session.compare(m['cells']['baseline']['object'], cell['object'], m['reference'])
                if comparison.get('shared_sized_functions', 0) <= 0:
                    raise ValueError('empty function denominator')
                write_new(record / (name + '-comparison.json'), comparison)
                row['comparison'] = {'status': 'measured', 'shared_sized_functions': comparison['shared_sized_functions'],
                                     'grade_changes': comparison['grade_changes'], 'inventory': comparison['inventory'],
                                     'sha256': digest(record / (name + '-comparison.json'))}
            except (OSError, ValueError) as exc:
                row['comparison'] = {'status': 'unknown', 'reason': str(exc)}
        result['cells'][name] = row
    result['freshness_errors'] = freshness(m['inputs'] + generated)
    result['interaction'] = {'complete': not result['freshness_errors'] and all(c['comparison']['status'] == 'measured' for c in result['cells'].values()),
                             'scope': 'Compare A and B individually with AB; no additive-benefit assumption.'}
    if result['interaction']['complete']:
        gains = {name: set(row['comparison']['grade_changes']['EXACT']['gained'])
                 for name, row in result['cells'].items()}
        result['interaction']['AB_only_exact_gains'] = sorted(gains['AB'] - gains['A'] - gains['B'])
        result['interaction']['individual_gains_absent_in_AB'] = sorted((gains['A'] | gains['B']) - gains['AB'])
        result['interaction']['AB_only_regalloc_gains'] = sorted(
            set(result['cells']['AB']['comparison']['grade_changes'].get('REGALLOC', {}).get('gained', []))
            - set(result['cells']['A']['comparison']['grade_changes'].get('REGALLOC', {}).get('gained', []))
            - set(result['cells']['B']['comparison']['grade_changes'].get('REGALLOC', {}).get('gained', [])))
    write_new(record / 'record.json', result)
    return result


def query(ledger, symbol=None, tu=None, hypothesis=None):
    rows = []
    for path in sorted(Path(ledger).glob('*/record.json')):
        record = json.loads(path.read_text())
        manifest = json.loads((path.parent / 'manifest.json').read_text())
        if any(value and value not in manifest[key] for key, value in
               [('symbol', symbol), ('tu', tu), ('hypothesis', hypothesis)]):
            continue
        errors = freshness(manifest['inputs'])
        errors += freshness([record['checker']]) if record.get('checker') else ['missing checker provenance']
        errors += freshness(record.get('analysis_inputs', []))
        if digest(path.parent / 'manifest.json') != record['manifest_sha256']:
            errors.append('manifest changed')
        for name, cell in record['cells'].items():
            for command in cell['commands']:
                if command.get('log'):
                    errors += freshness([{'path': command['log'], 'sha256': command['log_sha256']}])
                errors += freshness([{'path': p, 'sha256': sha} for p, sha in command['outputs'].items()])
            if cell['comparison'].get('sha256'):
                errors += freshness([{'path': str(path.parent / (name + '-comparison.json')),
                                      'sha256': cell['comparison']['sha256']}])
        rows.append({'ref': str(path.resolve()), 'record_sha256': digest(path),
                     'hypothesis': manifest['hypothesis'], 'symbol': manifest['symbol'], 'tu': manifest['tu'],
                     'freshness_errors': errors, 'cells': record['cells'], 'interaction': record['interaction']})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['run', 'query', 'export'])
    parser.add_argument('--manifest')
    parser.add_argument('--ledger', default='build/recovery-pilot/checker/ledger')
    parser.add_argument('--symbol')
    parser.add_argument('--tu')
    parser.add_argument('--hypothesis')
    args = parser.parse_args()
    try:
        if args.action == 'run':
            output = run(json.loads(Path(args.manifest).read_text()), args.ledger)
        else:
            rows = query(args.ledger, args.symbol, args.tu, args.hypothesis)
            output = {'schema': 'recovery-memory-packet-v1', 'records': rows,
                      'scope': 'Frozen retrieval evidence; verify refs and freshness before reuse.'} if args.action == 'export' else rows
        print(json.dumps(output, indent=2, sort_keys=True))
        if args.action == 'run':
            failed = output['freshness_errors'] or any(
                row['comparison']['status'] != 'measured' or
                any(c['status'] != 'passed' for c in row['commands']) or
                any(row[k]['status'] in ('failed', 'unknown') for k in ('period', 'linked_body'))
                for row in output['cells'].values())
            return 1 if failed else 0
        return 0
    except (OSError, ValueError, KeyError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
