#!/usr/bin/env python3
"""Audit V8 capture controls and follow actual word-load UIDs through GCC3."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_reload_trace import function, instructions
from gcc3_value_carriers_audit import inspect

TARGET = 'v8_fskdemodulate'


def walk(node):
    if isinstance(node, list):
        yield node
        for child in node[1:]:
            yield from walk(child)


def word_reads(pattern):
    return [n for setter in walk(pattern) if setter[0] == 'set'
            for n in walk(setter[2]) if n[0].split(':')[-1] == 'HI'
            and n[0].startswith('mem')]


def const(node, value):
    return any(n[0] == 'const_int' and n[1] == str(value) for n in walk(node))


def initial_pair(stream):
    pointer_regs = set()
    coeff = sample = None
    for uid, pattern in stream.items():
        for setter in walk(pattern):
            if setter[0] != 'set':
                continue
            dest, source = setter[1:3]
            if dest[0].startswith('reg') and any(n[0].startswith('mem') and const(n, 3544)
                                                for n in walk(source)):
                pointer_regs.add(dest[1])
        for mem in word_reads(pattern):
            if const(mem, 3572) and sample is None:
                sample = uid
            if any(n[0].startswith('reg') and n[1] in pointer_regs for n in walk(mem)) and coeff is None:
                coeff = uid
    assert coeff is not None and sample is not None
    return coeff, sample


def extension(stream, load):
    pattern = stream[load]
    assert pattern[0] == 'set' and pattern[1][0].startswith('reg')
    reg = pattern[1][1]
    for uid, p in stream.items():
        if p[0] == 'set' and p[2][0].startswith('sign_extend'):
            operand = p[2][1]
            if operand[0].startswith('reg') and operand[1] == reg:
                return uid
    raise AssertionError(('missing load extension', load))


def order(stream, coeff_ids, sample_ids):
    # Combine folds a HI load into its sign-extension's UID. Require a real
    # source memory read, not a surviving but register-only extension.
    ids = list(stream)
    coeff = [uid for uid in coeff_ids if uid in stream and word_reads(stream[uid])]
    sample = [uid for uid in sample_ids if uid in stream and word_reads(stream[uid])]
    if len(coeff) != 1 or len(sample) != 1:
        raise ValueError(('ambiguous/missing word reads', coeff, sample))
    return {'coefficient_uid': coeff[0], 'sample_uid': sample[0],
            'order': 'coefficient-first' if ids.index(coeff[0]) < ids.index(sample[0]) else 'sample-first'}


def main():
    root = d.ROOT/'build/v8-product-capture'
    cells = json.loads((root/'results.json').read_text())['families']['V8Dpsk']['cells']
    assert len(cells) == 4 and cells['baseline']['baseline_reproduced']
    basepath = root/'V8Dpsk/baseline/candidate.o'
    base = inspect(basepath)
    bodies = {n: d.b.body(str(basepath), n) for n in base['text_positions']}
    assert len(bodies) == 4
    reports = []
    for label, cell in cells.items():
        path = root/'V8Dpsk'/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(path)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == base[key], (label, key)
        assert set(current['text_positions']) == set(bodies)
        changed = [n for n, body in bodies.items() if d.b.body(str(path), n) != body]
        assert changed in ([], [TARGET])
        assert not cell.get('gains') and not cell.get('losses')
        for n in bodies:
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))) == cell['verdicts'][n]
        initial = instructions(function((path.parent/'V8Dpsk.c.01.rtl').read_text(), TARGET))
        coeff, sample = initial_pair(initial)
        coeff_ids = (coeff, extension(initial, coeff))
        sample_ids = (sample, extension(initial, sample))
        stages = {}
        diagnostic_dumps = {}
        for filename in sorted(path.parent.glob('V8Dpsk.c.[0-9][0-9].*')):
            if filename.name.endswith('.cgraph'):
                continue
            stage = filename.name.split('.c.', 1)[1]
            try:
                stream = instructions(function(filename.read_text(), TARGET))
            except ValueError as error:
                assert stage == '08.gcse', (stage, str(error))
                diagnostic_dumps[stage] = str(error)
                continue
            stages[stage] = order(stream, coeff_ids, sample_ids)
        cse = instructions(function((path.parent/'V8Dpsk.c.06.cse').read_text(), TARGET))
        sample_counts = {name: sum(const(mem, 3572) for pattern in stream.values() for mem in word_reads(pattern))
                         for name, stream in [('01.rtl', initial), ('06.cse', cse)]}
        assert sample_counts['01.rtl'] == (5 if label == 'coefficient-first-expressions' else 2), (label, sample_counts)
        assert sample_counts['06.cse'] == 2, (label, sample_counts)
        coefficient = label.startswith('coefficient-first')
        assert stages['01.rtl']['order'] == ('coefficient-first' if coefficient else 'sample-first')
        if coefficient:
            assert stages['31.bbro']['order'] == 'coefficient-first'
            assert stages['33.sched2']['order'] == 'sample-first'
            first_sample_stage = next(s for s, obs in stages.items() if obs['order'] == 'sample-first')
            assert first_sample_stage == '33.sched2'
        assert stages['35.mach']['order'] == 'sample-first'
        # Known refusal: a live register extension is not a memory capture.
        register_only = {coeff: ['set', ['reg:HI', '1000'], ['reg:HI', '1001']],
                         sample: initial[sample]}
        try:
            order(register_only, (coeff,), (sample,))
        except ValueError:
            pass
        else:
            raise AssertionError('register-only capture accepted')
        reports.append({'label': label, 'size': current['text_positions'][TARGET][1],
                        'verdict': cell['verdicts'][TARGET], 'changed': changed,
                        'sample_read_counts': sample_counts, 'stages': stages, 'unparsed_diagnostic_dumps': diagnostic_dumps})
    expressions = root/'V8Dpsk/coefficient-first-expressions/candidate.o'
    owner = root/'V8Dpsk/coefficient-first-owner/candidate.o'
    assert expressions.read_bytes() == owner.read_bytes()
    previous = d.ROOT.parent/'v8-receive-boundary/build/v8-index-cursor/V8Dpsk/unsigned-index-and-cursor/candidate.o'
    control = root/'V8Dpsk/index-cursor-control/candidate.o'
    assert cells['index-cursor-control']['object_hash'] == 'c4891798ce0453bd52f984f9fb279257a3b495a329d64bf0f85ac6801f0426e4'
    if previous.exists():
        assert previous.read_bytes() == control.read_bytes()
    result = {'cells': 4, 'emitted_body_comparisons': 16, 'common_verdicts': 16,
              'stage_observations': sum(len(r['stages']) for r in reports),
              'coefficient_forms_raw_equal': True, 'prior_control_repeat': True,
              'register_only_refusal': True, 'gains': [], 'losses': [], 'reports': reports}
    (root/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f"V8 capture audit: 4 cells, 16 bodies, 16 common verdicts, {result['stage_observations']} stage observations; "
          'metadata/nontext/bystanders equal; 0 gains/losses; first reversal33.sched2')


if __name__ == '__main__':
    main()
