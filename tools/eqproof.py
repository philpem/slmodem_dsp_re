#!/usr/bin/env python3
"""Equivalence-evidence TRIAGE, never a proof or a reconstruction gate.

Port of the useful census/collector ideas from issue #14's archived tooling.
DIRECT means a compiled test references ref_SYMBOL, not that it compares or
executes it. COMPOSITE means a potential relocation path from such a reference;
NONE means no path found by this incomplete static model, not no testing.
Checks are per binary, not inputs, coverage, or an exhaustiveness claim.
Grade 1 is byteident's linear register-renaming heuristic, not semantic proof.
UNRESOLVED relocations are never EXACT. Every COMDAT copy is considered.

Prerequisites (not run here): make tc; build the differential objects/binaries.
Collect: sh tools/eqproof-checks.sh build
Census: python3 tools/eqproof.py [--class DIRECT|COMPOSITE|NONE] [--sym NAME]
Period collection: sh tools/eqproof-checks.sh build/period --layout period
Period census: python3 tools/eqproof.py build/period --layout period
The period collector records ELF compiler banners; these do not recover flags
or identify which compiler contributed each linked byte. Retain the build log.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
# tools/dis.py must not shadow Python's standard library module.
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != HERE]
sys.path.insert(0, str(HERE / 'toolchain'))
import byteident as BI


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def make_list(name):
    text = subprocess.run(['make', '-s', '--no-print-directory', 'print-' + name],
                          cwd=ROOT, check=True, capture_output=True, text=True).stdout
    values = text.split()
    require(values and len(values) == len(set(values)), 'empty/duplicate ' + name)
    return values


def test_names():
    names = make_list('TESTS') + make_list('CXXTESTS')
    require(len(names) == len(set(names)), 'duplicate C/C++ test binary names')
    return names


def source_identity():
    """Content identity of the scoped inputs, including untracked test sources."""
    inputs = [ROOT / 'Makefile']
    for base in ('src', 'include', 'test/unit', 'test/harness', 'tools/toolchain'):
        inputs.extend(p for p in (ROOT / base).rglob('*') if p.is_file() and
                      p.suffix in ('.c', '.cpp', '.h', '.hpp', '.inc', '.S', '.sh', '.mk'))
    require(inputs, 'zero source identity inputs')
    content = '\n'.join(str(p.relative_to(ROOT)) + ' ' + digest(p) for p in sorted(inputs))
    return dict(files=len(inputs), sha256=hashlib.sha256(content.encode()).hexdigest())


def parse_checks(text):
    """Only exact harness verdicts count; malformed verdicts invalidate a run."""
    sections = checks = failed = 0
    malformed = []
    for line in text.splitlines():
        good = re.fullmatch(r'PASS\s+.+?\s+(\d+) checks', line)
        bad = re.fullmatch(r'FAIL\s+.+?\s+(\d+)/(\d+) checks failed', line)
        if good:
            sections += 1
            checks += int(good[1])
        elif bad and 0 < int(bad[1]) <= int(bad[2]):
            sections += 1
            failed += int(bad[1])
            checks += int(bad[2])
        elif line.startswith(('PASS ', 'FAIL ')):
            malformed.append(line)
    return dict(sections=sections, checks=checks, failed_checks=failed,
                malformed=malformed)


def compiler_comment(path):
    # Missing .comment is explicitly unknown; failure of readelf is fatal.
    text = run('readelf', '-p', '.comment', str(path))
    return [line.split(']', 1)[1].strip() for line in text.splitlines()
            if re.match(r'\s*\[\s*[0-9a-f]+\]', line)]


def measure(binary, directory, timeout):
    """Keep failures even when they print no verdict, crash, or time out."""
    before = digest(binary)
    stdout = directory / (binary.name + '.stdout')
    stderr = directory / (binary.name + '.stderr')
    error = None
    rc = None
    with stdout.open('wb') as out, stderr.open('wb') as err:
        try:
            proc = subprocess.run([str(binary)], cwd=ROOT, stdout=out, stderr=err,
                                  timeout=timeout)
            rc = proc.returncode
        except (OSError, subprocess.TimeoutExpired) as exc:
            error = str(exc)
    row = parse_checks(stdout.read_text(errors='replace'))
    row.update(binary=str(binary), sha256=before, returncode=rc, error=error,
               stdout=str(stdout), stderr=str(stderr),
               stdout_sha256=digest(stdout), stderr_sha256=digest(stderr))
    row['changed_during_run'] = before != digest(binary)
    return row


def successful(row):
    return (row['returncode'] == 0 and not row['error'] and row['checks'] > 0
            and row['sections'] > 0 and not row['failed_checks']
            and not row['malformed'] and not row['changed_during_run'])


def collect(build, layout, timeout, names=None):
    require(build.is_dir(), 'missing build directory: ' + str(build))
    names = test_names() if names is None else names
    require(names, 'zero expected binaries')
    bindir = build / 'test' if layout == 'modern' else build
    # A unique run directory prevents an interrupted rerun from destroying logs.
    directory = Path(tempfile.mkdtemp(prefix='eqproof-', dir=build))
    packet = dict(schema=1, purpose='triage; not proof', layout=layout,
                  build=str(build), revision=run('git', '-C', str(ROOT), 'rev-parse', 'HEAD').strip(),
                  command=sys.argv, started=time.time(), expected=names, binaries={},
                  source_identity=source_identity(), missing=[], complete=False)
    packet['blob_sha256'] = digest(BI.BLOB) if Path(BI.BLOB).is_file() else None
    output = build / 'eqproof_checks.json'

    def save():
        temporary = directory / 'packet.json'
        temporary.write_text(json.dumps(packet, indent=2) + '\n')
        # Atomic publish; raw logs from all previous runs remain intact.
        temporary.replace(output)

    save()
    for name in names:
        binary = bindir / name
        if not binary.is_file() or not os.access(binary, os.X_OK):
            packet['missing'].append(name)
        else:
            row = measure(binary, directory, timeout)
            try:
                row['compiler_comments'] = compiler_comment(binary)
            except (OSError, subprocess.CalledProcessError) as exc:
                row['provenance_error'] = str(exc)
            packet['binaries'][name] = row
        save()
    packet['complete'] = True
    packet['source_changed_during_run'] = packet['source_identity'] != source_identity()
    packet['finished'] = time.time()
    save()
    rows = packet['binaries'].values()
    passed = sum(successful(r) for r in rows)
    print('collector: %d/%d binaries run; %d successful, %d unsuccessful, %d missing; %d checks, %d failed checks'
          % (len(packet['binaries']), len(names), passed,
             len(packet['binaries']) - passed, len(packet['missing']),
             sum(r['checks'] for r in rows), sum(r['failed_checks'] for r in rows)))
    print('evidence:', output)
    return 0 if passed == len(names) and not packet['source_changed_during_run'] else 1


def reachable(graph, roots):
    seen, work = set(roots), list(roots)
    while work:
        for target in graph.get(work.pop(), ()):
            if target not in seen:
                seen.add(target)
                work.append(target)
    return seen


def evidence_class(symbol, direct, reached):
    return 'DIRECT' if symbol in direct else 'COMPOSITE' if symbol in reached else 'NONE'


def reference_map(objects):
    direct = {}
    for name, path in objects.items():
        for line in run('nm', '-u', str(path)).splitlines():
            fields = line.split()
            if len(fields) == 2 and fields[0] == 'U' and fields[1].startswith('ref_'):
                direct.setdefault(fields[1][4:], set()).add(name)
    require(direct, 'zero compiled ref_ references')
    return direct


def graph_from_objects(objects):
    # Named relocations only. Local resolved calls, section targets and indirect
    # dispatch are incomplete. Address-taking is not evidence of execution.
    graph = {}
    for path in objects:
        current = None
        for line in run('objdump', '-dr', '--no-show-raw-insn', str(path)).splitlines():
            match = re.match(r'^[0-9a-f]+ <([^>]+)>:', line)
            if match:
                current = match[1]
                graph.setdefault(current, set())
            reloc = re.match(r'^\s*[0-9a-f]+:\s+R_386_\w+\s+(\S+)', line)
            if reloc and current:
                target = re.split(r'[+-]0x', reloc[1])[0]
                if not target.startswith('.'):
                    graph[current].add(target)
    require(graph, 'zero functions in relocation graph')
    return graph


def load_checks(path, names, build, layout, objects, report_incomplete=False):
    packet = json.loads(path.read_text())
    require(packet.get('schema') == 1 and packet.get('complete'), 'incomplete/unknown check packet')
    require(packet['build'] == str(build) and packet['layout'] == layout, 'check packet build/layout mismatch')
    require(set(packet['expected']) == set(names), 'check packet test census changed')
    require(not packet['missing'] and set(packet['binaries']) == set(names), 'missing binary evidence')
    packet['invalid_denominators'] = []
    for name, row in packet['binaries'].items():
        binary = build / 'test' / name if layout == 'modern' else build / name
        require(row['binary'] == str(binary) and digest(binary) == row['sha256'], 'binary changed: ' + name)
        require(binary.stat().st_mtime_ns >= objects[name].stat().st_mtime_ns, 'stale binary: ' + name)
        for stream in ('stdout', 'stderr'):
            require(digest(row[stream]) == row[stream + '_sha256'], 'log changed: ' + name)
        require(parse_checks(Path(row['stdout']).read_text(errors='replace')) ==
                {key: row[key] for key in ('checks', 'sections', 'failed_checks', 'malformed')},
                'check totals disagree with raw log: ' + name)
        valid = row['checks'] > 0 and row['sections'] > 0 and not row['malformed']
        if not valid:
            require(report_incomplete, 'zero/malformed check denominator: ' + name)
            packet['invalid_denominators'].append(name)
        require(not row['changed_during_run'], 'binary changed during collection: ' + name)
    return packet


def grade_copies(reference, copies):
    """Conservative all-copy census; do not let one lucky COMDAT copy win."""
    grades = []
    for body, rows in copies:
        verdict, _ = BI.verdict(*reference[0], *body)
        require(verdict != 'NODATA', 'missing disassembly bytes')
        if verdict not in ('EXACT', 'UNRESOLVED', 'RELOC'):
            require(reference[1] and rows, 'zero instruction denominator')
            if BI.alpha_equal(reference[1], rows):
                verdict = 'REGALLOC'
        grades.append(verdict)
    return combine_grades(grades)


def combine_grades(grades):
    require(grades, 'zero defining copies')
    if all(g == 'EXACT' for g in grades):
        return 'EXACT'
    if all(g in ('EXACT', 'REGALLOC') for g in grades):
        return 'REGALLOC'
    if any(g not in ('EXACT', 'REGALLOC', 'UNRESOLVED') for g in grades):
        return 'OTHER'
    return 'UNRESOLVED'


def select_population(rows, grade1=False):
    require(rows, 'zero common symbols')
    return [r for r in rows if r[1] == 'REGALLOC'] if grade1 else [r for r in rows if r[1] != 'EXACT']


def outcome_status(packet):
    require(packet['binaries'], 'zero outcome binaries')
    if packet['invalid_denominators']:
        return 2
    return 0 if all(successful(row) for row in packet['binaries'].values()) else 1


def census(args, build):
    tc = Path(BI.OURS).resolve()
    blob = Path(BI.BLOB).resolve()
    config = (tc / '.build-config').read_text()
    require('DSPLIB_REPRODUCE_BUGS' in config, 'tc config lacks bug reproduction define')
    manifest = [line.split() for line in (tc / 'tc_manifest.txt').read_text().splitlines()]
    require(manifest and all(len(r) == 2 for r in manifest), 'empty/malformed tc manifest')
    sources = make_list('SRC') + make_list('CXXSRC')
    require(set(r[1] for r in manifest) == set(sources), 'tc manifest/source census mismatch; run make tc')
    require(len(manifest) == len(sources), 'duplicate manifest rows')
    objects = [tc / row[0] for row in manifest]
    require(set(objects) == set(tc.glob('*.o')), 'partial/extra tc objects; run make tc')
    headers = [p for base in ('src', 'include') for p in (ROOT / base).rglob('*')
               if p.suffix in ('.h', '.hpp', '.inc')]
    newest_header = max(p.stat().st_mtime_ns for p in headers)
    for obj, (_, source) in zip(objects, manifest):
        require(obj.stat().st_mtime_ns >= max(newest_header, (ROOT / source).stat().st_mtime_ns,
                                             (tc / '.build-config').stat().st_mtime_ns),
                'stale tc object: ' + str(obj))
    names = test_names()
    test_headers = list((ROOT / 'test/harness').glob('*.h'))
    newest_test_header = max([newest_header] + [p.stat().st_mtime_ns for p in test_headers])
    test_objects = {}
    for name in names:
        obj = build / 'test' / 'unit' / (name + '.o') if args.layout == 'modern' else build / ('test_unit_' + name + '.o')
        require(obj.is_file(), 'missing test object: ' + str(obj))
        source = next((p for p in ((ROOT / 'test/unit' / (name + '.c')),
                                  (ROOT / 'test/unit' / (name + '.cpp'))) if p.is_file()), None)
        require(source is not None and obj.stat().st_mtime_ns >= max(newest_test_header, source.stat().st_mtime_ns),
                'stale test object: ' + str(obj))
        test_objects[name] = obj
    packet = load_checks(build / 'eqproof_checks.json', names, build, args.layout, test_objects,
                         report_incomplete=True)
    require(packet.get('blob_sha256') == digest(blob), 'missing/changed blob identity in check packet')
    require(not packet.get('source_changed_during_run') and packet.get('source_identity') == source_identity(),
            'source inputs changed since/during collection; rebuild and recollect')
    require(all(row.get('compiler_comments') and not row.get('provenance_error')
                for row in packet['binaries'].values()), 'missing binary compiler provenance')
    direct = reference_map(test_objects)
    linked_objects = []
    for source in sources:
        obj = (build / 'repro' / Path(source).relative_to('src').with_suffix('.o')
               if args.layout == 'modern' else build / (str(Path(source).with_suffix('')).replace('/', '_') + '.o'))
        require(obj.is_file() and obj.stat().st_mtime_ns >= max(newest_header, (ROOT / source).stat().st_mtime_ns),
                'missing/stale differential object: ' + str(obj))
        linked_objects.append(obj)
    harness_objects = []
    for source in make_list('HARNESS'):
        obj = (build / Path(source).with_suffix('.o') if args.layout == 'modern'
               else build / (str(Path(source).with_suffix('')).replace('/', '_') + '.o'))
        require(obj.is_file() and obj.stat().st_mtime_ns >= max(newest_test_header, (ROOT / source).stat().st_mtime_ns),
                'missing/stale harness object: ' + str(obj))
        harness_objects.append(obj)
    # All source objects are linked into every differential binary. Do not
    # accept a freshly re-collected old executable after a source rebuild.
    newest_input = max(p.stat().st_mtime_ns for p in linked_objects + harness_objects)
    for name, row in packet['binaries'].items():
        require(Path(row['binary']).stat().st_mtime_ns >= newest_input, 'stale linked binary: ' + name)
    graph = graph_from_objects(linked_objects)
    reached = reachable(graph, direct)
    blob_sizes = BI.sizes(str(blob))
    require(blob_sizes, 'zero blob symbols')
    definitions = {}
    comments = set()
    for obj in objects:
        comments.update(compiler_comment(obj))
        for symbol in BI.sizes(str(obj)):
            definitions.setdefault(symbol, []).append(obj)
    common = sorted(set(blob_sizes) & set(definitions))
    require(common, 'zero common symbols')
    require(comments, 'missing tc compiler provenance')
    print('TRIAGE ONLY: static references and register renaming do not prove equivalence.')
    if packet['invalid_denominators']:
        print('INCOMPLETE outcome evidence: %d/%d binaries have zero/malformed denominators; census will exit 2: %s' %
              (len(packet['invalid_denominators']), len(names), ', '.join(packet['invalid_denominators'])))
    print('blob:', blob, 'sha256:', digest(blob), 'compiler:', sorted(set(compiler_comment(blob))))
    print('code grades from TC_OUT:', tc, 'compiler:', sorted(comments))
    print('outcomes and static references from selected layout:', args.layout, build)
    if args.layout == 'modern':
        print('WARNING: modern outcomes are from a separate build, not the TC_OUT period compiler; do not attribute these failures or passes to its code grades.')
    print('analysis tools:', run('objdump', '--version').splitlines()[0], ';', run('nm', '--version').splitlines()[0])
    print('test layout:', args.layout, 'compiler banners:', sorted({c for r in packet['binaries'].values() for c in r.get('compiler_comments', [])}))
    print('Compiler banners are provenance clues, not recovered commands; retain build logs.')
    print('tc .build-config sha256:', digest(tc / '.build-config'))
    print(config.rstrip())
    print('census: %d/%d source objects; %d/%d test objects; %d/%d blob symbols compared; %d absent'
          % (len(objects), len(sources), len(test_objects), len(names), len(common), len(blob_sizes), len(set(blob_sizes) - set(common))))
    print('potential graph: %d functions, %d named relocation edges' % (len(graph), sum(map(len, graph.values()))))
    rows = []
    for symbol in common:
        ref = (BI.body(str(blob), symbol), BI.insns(str(blob), symbol))
        copies = [(BI.body(str(obj), symbol), BI.insns(str(obj), symbol)) for obj in definitions[symbol]]
        grade = grade_copies(ref, copies)
        rows.append((symbol, grade, evidence_class(symbol, direct, reached)))
    for grade in ('EXACT', 'REGALLOC', 'UNRESOLVED', 'OTHER'):
        print('%s: %d/%d compared symbols' % (grade, sum(r[1] == grade for r in rows), len(rows)))
    population = select_population(rows, args.grade1)
    if not population:
        print('No selected symbols: the full comparison census is nonzero; this selection is empty.')
    for klass in ('DIRECT', 'COMPOSITE', 'NONE'):
        print('%s: %d/%d selected symbols' % (klass, sum(r[2] == klass for r in population), len(population)))
    if args.sym:
        require(args.sym in common, 'symbol not in comparison denominator: ' + args.sym)
    for symbol, grade, klass in rows:
        if args.sym == symbol or (not args.sym and args.klass == klass and (symbol, grade, klass) in population):
            print(symbol, grade, klass)
            for name in sorted(direct.get(symbol, ())):
                row = packet['binaries'][name]
                print('  %s: %d binary-wide checks, %d failed; rc=%s' % (name, row['checks'], row['failed_checks'], row['returncode']))
    passed = sum(successful(row) for row in packet['binaries'].values())
    print('collected outcomes: %d/%d successful binaries; %d checks (not input vectors)' %
          (passed, len(names), sum(r['checks'] for r in packet['binaries'].values())))
    return outcome_status(packet)


def selftest():
    tests = []
    def check(name, condition):
        tests.append(bool(condition))
        print('%s %s' % ('ok' if condition else 'FAIL', name))
    check('FAIL denominator retained', parse_checks('PASS a 3 checks\nFAIL b 2/7 checks failed\n')['checks'] == 10)
    check('malformed verdict fires', bool(parse_checks('FAIL b 8/7 checks failed')['malformed']))
    check('empty output has zero denominator', parse_checks('')['checks'] == 0)
    graph = {'root': {'child'}, 'child': {'leaf'}, 'leaf': {'root'}}
    check('composite detector fires', evidence_class('leaf', {'root': {'test'}}, reachable(graph, ['root'])) == 'COMPOSITE')
    check('no-root negative control', evidence_class('leaf', {}, reachable(graph, [])) == 'NONE')
    check('direct takes precedence', evidence_class('root', {'root': {'test'}}, reachable(graph, ['root'])) == 'DIRECT')
    with tempfile.TemporaryDirectory(prefix='eqproof-selftest-') as tmp:
        directory = Path(tmp)
        assembly = directory / 'references.s'
        assembly.write_text('.text\n.globl root, child\n.extern ref_unused\n'
                            'root:\ncall ref_leaf\ncall child\nret\nchild:\nret\n')
        obj = directory / 'references.o'
        run('as', '--32', '-o', str(obj), str(assembly))
        direct = reference_map({'fixture': obj})
        check('compiled reference fires; unused extern does not', direct == {'leaf': {'fixture'}})
        compiled_graph = graph_from_objects([obj])
        check('compiled relocation path fires', 'child' in reachable(compiled_graph, ['root']) and
              'child' not in reachable(compiled_graph, []))
        for name, script, want, count in (
                ('pass', 'printf "PASS fixture 4 checks\\n"', True, 4),
                ('fail', 'printf "FAIL fixture 2/7 checks failed\\n"; exit 7', False, 7),
                ('silent', 'exit 0', False, 0),
                ('masked', 'printf "PASS fixture 4 checks\\n"; exit 9', False, 4),
                ('falsepass', 'printf "FAIL fixture 1/4 checks failed\\n"', False, 4),
                ('stderr', 'printf "diagnostic\\n" >&2; exit 3', False, 0),
                ('timeout', 'exec sleep 2', False, 0)):
            binary = directory / name
            binary.write_text('#!/bin/sh\n' + script + '\n')
            binary.chmod(0o700)
            row = measure(binary, directory, 0.05 if name == 'timeout' else 2)
            check('collector ' + name, successful(row) == want and row['checks'] == count)
            if name == 'stderr':
                check('stderr evidence preserved', Path(row['stderr']).read_text() == 'diagnostic\n' and row['returncode'] == 3)
        status = collect(directory, 'period', 2, ['pass', 'fail'])
        packet = load_checks(directory / 'eqproof_checks.json', ['pass', 'fail'], directory,
                             'period', {n: directory / n for n in ('pass', 'fail')})
        check('failed packet remains readable and nonzero', status == 1 and
              packet['binaries']['fail']['returncode'] == 7 and
              packet['binaries']['fail']['checks'] == 7)
        Path(packet['binaries']['pass']['stdout']).write_text('PASS forged 99 checks\n')
        try:
            load_checks(directory / 'eqproof_checks.json', ['pass', 'fail'], directory,
                        'period', {n: directory / n for n in ('pass', 'fail')})
        except ValueError:
            check('changed evidence detector fires', True)
        else:
            check('changed evidence detector fires', False)
        collect(directory, 'period', 2, ['silent'])
        partial = load_checks(directory / 'eqproof_checks.json', ['silent'], directory,
                              'period', {'silent': directory / 'silent'}, report_incomplete=True)
        check('partial census retains zero denominator and exits 2',
              partial['invalid_denominators'] == ['silent'] and outcome_status(partial) == 2)
        try:
            load_checks(directory / 'eqproof_checks.json', ['silent'], directory,
                        'period', {'silent': directory / 'silent'})
        except ValueError:
            check('strict zero-denominator load still refuses', True)
        else:
            check('strict zero-denominator load still refuses', False)
        status = collect(directory, 'period', 2, ['absent'])
        packet = json.loads((directory / 'eqproof_checks.json').read_text())
        check('zero runnable binaries refuses with denominator', status == 1 and
              packet['missing'] == ['absent'] and not packet['binaries'])
        try:
            load_checks(directory / 'eqproof_checks.json', ['absent'], directory, 'period', {})
        except ValueError:
            check('missing evidence census refuses', True)
        else:
            check('missing evidence census refuses', False)
    reference = (([1], {}), [('mov', '%eax,%eax')])
    check('exact control', grade_copies(reference, [reference]) == 'EXACT')
    different = (([2, 3], {}), [('ret', '')])
    check('COMDAT nonexact copy fires', grade_copies(reference, [reference, different]) == 'OTHER')
    for known in ('RELOC', 'BYTES', 'SIZE'):
        check('known ' + known + ' dominates unresolved', combine_grades(['UNRESOLVED', known]) == 'OTHER')
    check('unresolved remains unknown', combine_grades(['EXACT', 'UNRESOLVED']) == 'UNRESOLVED')
    check('all-exact selection is legitimately empty', select_population([('s', 'EXACT', 'DIRECT')]) == [])
    check('zero REGALLOC selection is legitimately empty', select_population([('s', 'OTHER', 'DIRECT')], True) == [])
    try:
        select_population([])
    except ValueError:
        check('zero full census still refuses', True)
    else:
        check('zero full census still refuses', False)
    try:
        grade_copies(reference, [])
    except ValueError:
        check('zero copies refuse', True)
    else:
        check('zero copies refuse', False)
    print('eqproof self-checks: %d/%d passed, %d failed' % (sum(tests), len(tests), len(tests) - sum(tests)))
    return 0 if all(tests) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('build', nargs='?', default=os.environ.get('EQPROOF_BUILD', str(ROOT / 'build')))
    parser.add_argument('--layout', choices=('modern', 'period'), default='modern')
    parser.add_argument('--collect', action='store_true')
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--selftest', '--self-test', action='store_true')
    parser.add_argument('--class', dest='klass', choices=('DIRECT', 'COMPOSITE', 'NONE'))
    parser.add_argument('--sym')
    parser.add_argument('--grade1', action='store_true', help='select REGALLOC triage population; no proof claim')
    args = parser.parse_args()
    try:
        require(args.timeout > 0, 'timeout must be positive')
        if args.selftest:
            return selftest()
        build = Path(args.build).resolve()
        return collect(build, args.layout, args.timeout) if args.collect else census(args, build)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as exc:
        print('eqproof: REFUSED: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
