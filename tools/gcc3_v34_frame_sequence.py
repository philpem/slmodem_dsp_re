#!/usr/bin/env python3
"""Reproduce witnessed V34 frame groups, callback capture and shift chain."""
import itertools
import hashlib
import json
import sys
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked
from gcc3_getframe_callback_expansion import variants as get_variants
from gcc3_v34_putframe_expansion import variants as put_variants


def variants(path, source):
    cells = {'baseline': source}
    cells['put-fixed-int'] = put_variants(path, source)['expand-1-nbword-0-smallword-0']
    fixed = get_variants(path, source)['expand-1-capture-1-word-0']
    old = '''\t\tp[0] = (short)(v & 1);
\t\tp[1] = (short)((v >> 1) & lsbMask[sw]);
\t\tp[2] = (short)((v >> ((1 + sw) & 31)) & lsbMask[w & 31]);
\t\tp[3] = (short)((v >> ((1 + sw + w) & 31)) & lsbMask[w & 31]);'''
    new = '''\t\tp[0] = (short)(v & 1);
\t\tv >>= 1;
\t\tp[1] = (short)(v & lsbMask[sw]);
\t\tv >>= sw & 31;
\t\tp[2] = (short)(v & lsbMask[w & 31]);
\t\tv >>= w & 31;
\t\tp[3] = (short)(v & lsbMask[w & 31]);'''
    assert fixed.count(old) == 4
    for sequence, nbword, posword in itertools.product((False, True), repeat=3):
        a, z, fn = d.function(fixed, 'getFrame')
        if sequence:
            fn = fn.replace(old, new)
        if nbword:
            fn = fn.replace('\tint nb;', '\tshort nb;')
        if posword:
            fn = fn.replace('\tint pos;', '\tshort pos;')
        label = 'get-fixed-sequence-%d-nbword-%d-posword-%d' % (sequence, nbword, posword)
        cells[label] = fixed[:a] + fn + fixed[z:]
    assert len(cells) == len(set(cells.values())) == 10
    reference = cells['get-fixed-sequence-1-nbword-0-posword-0']
    a, z, fn = d.function(reference, 'getFrame')
    fn = fn.replace('\tint pos;', '\tint pos;\n\tunsigned bitbuf;')
    marker = '''\t\tif (pos > 15)
\t\t\tpos = scramble(objp, (short)pos);

\t\ts->bitpos = (short)pos;
\t\tv = (unsigned)s->bitbuf >> (pos & 31);'''
    assert fn.count(marker) == 4
    first = '''\t\tif (pos > 15)
\t\t\tpos = scramble(objp, (short)pos);

\t\ts->bitpos = (short)pos;
\t\tbitbuf = (unsigned)s->bitbuf;
\t\tv = bitbuf >> (pos & 31);'''
    later = '''\t\tif (pos > 15) {
\t\t\tpos = scramble(objp, (short)pos);
\t\t\tbitbuf = (unsigned)s->bitbuf;
\t\t}

\t\ts->bitpos = (short)pos;
\t\tv = bitbuf >> (pos & 31);'''
    fn = fn.replace(marker, first, 1).replace(marker, later)
    cells['get-fixed-sequence-1-buffer-capture'] = reference[:a] + fn + reference[z:]
    assert len(cells) == len(set(cells.values())) == 11
    return cells


def shift_groups(path):
    shifts = [(m, o) for m, o in d.b.insns(str(path), 'getFrame')
              if m.startswith('shr')]
    groups = [shifts[i:i + 3] for i, (m, o) in enumerate(shifts)
              if o.startswith('$1,')]
    return groups, sum(len(group) == 3 and
                      len({o.split(',', 1)[1] for m, o in group}) == 1
                      for group in groups)


def buffer_refreshes(path, owner_operand):
    instructions = d.b.insns(str(path), 'getFrame')
    calls = [i for i, (m, o) in enumerate(instructions)
             if m == 'call' and o.startswith('*')]
    reads = [i for i, (m, o) in enumerate(instructions)
             if m == 'mov' and owner_operand in o]
    return {'owner_read_rows': reads,
            'last_three_callback_rows': calls[-3:],
            'refresh_within_four_instructions':
                [any(i < read <= i + 4 for read in reads) for i in calls[-3:]]}


def audit():
    root = d.ROOT / 'build/v34-frame-sequence'
    ledger = json.loads((root / 'results.json').read_text())
    cells = ledger['families']['v34shell']['cells']
    baseline = root / 'v34shell/baseline/candidate.o'
    metadata, data = inspect(baseline), allocated(baseline)
    original_groups, original_count = shift_groups(d.b.BLOB)
    assert original_count == len(original_groups) == 4
    original_buffer = buffer_refreshes(d.b.BLOB, '0x2a60(')
    assert original_buffer['refresh_within_four_instructions'] == [True] * 3
    rows = {}
    for label, cell in cells.items():
        path = root / 'v34shell' / label / 'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert inspect(path) == metadata
        assert allocated(path) == data
        assert not cell.get('gains') and not cell.get('losses')
        changed = [name for name in cell['functions']
                   if masked(path, name) != masked(baseline, name)]
        expected = [] if label == 'baseline' else ['decodeDepth', 'putFrame'] if label == 'put-fixed-int' else ['getFrame']
        assert changed == expected, (label, changed)
        get = d.b.insns(str(path), 'getFrame')
        put = d.b.insns(str(path), 'putFrame')
        get_calls = [o for m, o in get if m == 'call' and o.startswith('*')]
        put_calls = [o for m, o in put if m == 'call' and o.startswith('*')]
        captured = label.startswith('get-fixed-')
        mentions = [o for m, o in get if '0xe48(' in o]
        assert len(get_calls) == (6 if captured else 3)
        assert len(put_calls) == (18 if label == 'put-fixed-int' else 6)
        assert len(mentions) == (1 if captured else 3)
        if captured:
            assert all('(%esp)' in o for o in get_calls)
        groups, count = shift_groups(path)
        if captured:
            assert count == (4 if 'sequence-1-' in label else 0), (label, groups)
        buffer = buffer_refreshes(path, '0xe80(')
        if label in ('get-fixed-sequence-1-nbword-0-posword-0',
                     'get-fixed-sequence-1-buffer-capture'):
            expected_refresh = [label.endswith('buffer-capture')] * 3
            assert buffer['refresh_within_four_instructions'] == expected_refresh
            assert len(buffer['owner_read_rows']) == 7
        rows[label] = {'get_verdict': cell['verdicts']['getFrame'],
                       'put_verdict': cell['verdicts']['putFrame'],
                       'canonical_changed_bodies': changed,
                       'metadata_and_allocated_data_unchanged': True,
                       'get_callback_reads': mentions,
                       'get_calls': get_calls, 'put_calls': put_calls,
                       'shift_groups': groups, 'same_register_shift_chains': count,
                       'buffer_refresh_control': buffer,
                       'exact_gains': [], 'exact_losses': []}
    denominator = {'complete_TU_cells': len(cells),
                   'common_body_verdicts': sum(len(c['verdicts']) for c in cells.values())}
    assert denominator == {'complete_TU_cells': 11, 'common_body_verdicts': 132}
    (root / 'audit.json').write_text(json.dumps({'denominator': denominator,
        'original_shift_groups': original_groups,
        'original_buffer_refresh_control': original_buffer,
        'cells': rows}, indent=2) + '\n')
    print('11 full-TU cells /132 common body verdicts; metadata/data and callback/shift controls pass; zero exact gains/losses')


if __name__ == '__main__':
    if sys.argv[1:] == ['--audit-only']:
        audit()
        sys.exit(0)
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'f420cd89'
    d.OUT_NAME = 'v34-frame-sequence'
    d.SOURCE_PATHS = ('src/pump/v34/v34shell.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
    audit()
