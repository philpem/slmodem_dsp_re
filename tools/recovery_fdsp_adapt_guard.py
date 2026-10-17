#!/usr/bin/env python3
"""Cross the original empty-tap boundary with adaptation-product ownership.

A 2x2 source domain plus two sequential operand-backed controls, unchanged period flags.
Initial RTL already places the
baseline g product before the loop guard; no compiler motion causes that
ordering. Moving the product into the loop tests natural invariant motion.
The prediction is a guarded preheader product; an unguarded product falsifies it.
Original instructions at ae9b3/ae9b5 guard ae9b7/ae9b9, also independently
observed in the calibrated PR293 reference Ghidra export (000be8c0).
"""
import json
import playbook_small_patterns as d
from gcc3_value_carriers_audit import function_chunk
from recovery_fax_debug_audit import Object
from recovery_fixedrc_apparatus import allocated
from recovery_generator_separation import named_data

def variants(path, source):
    a, z, fn = d.function(source, 'EchoCanceler')
    old = """					float g = e * mu;
					unsigned int m;

					for (m = 0; m < ntaps; m++)
						coef[m] += g * hist[pos + m];"""
    assert fn.count(old) == 1
    result = {}
    for guarded in (False, True):
        for inside in (False, True):
            if inside:
                block = """					unsigned int m;

					for (m = 0; m < ntaps; m++) {
						float g = e * mu;
						coef[m] += g * hist[pos + m];
					}"""
            else:
                block = old
            if guarded:
                block = '\t\t\t\t\tif (ntaps != 0) {\n' + '\n'.join('\t'+s for s in block.splitlines()) + '\n\t\t\t\t\t}'
            label = 'baseline' if not guarded and not inside else ('guard-' if guarded else '') + ('product-inside' if inside else 'product-before')
            result[label] = source[:a] + fn.replace(old, block) + source[z:]
    # The reference duplicates still-live mu before consuming e in FMULP;
    # the loop-owned e*mu cell updates dead e with nonpopping FMUL instead.
    # Cross that specific destructive-operand choice after closing the 2x2.
    result['product-inside-mu-first'] = result['product-inside'].replace(
        'float g = e * mu;', 'float g = mu * e;')
    # One final two-cell consumer cross: reference loads g then FMULS history;
    # the mu-first cell loads history then FMULs the live g register instead.
    result['product-inside-mu-first-history-first'] = result['product-inside-mu-first'].replace(
        'coef[m] += g * hist[pos + m];', 'coef[m] += hist[pos + m] * g;')
    assert len(result) == len(set(result.values())) == 6
    return result

def audit():
    root = d.ROOT / 'build' / d.OUT_NAME / 'Fdspkrnl'
    before = root / 'baseline' / 'candidate.o'
    baseline = Object(before).canonical()
    records = {}
    for label in ('product-inside', 'guard-product-before', 'guard-product-inside', 'product-inside-mu-first', 'product-inside-mu-first-history-first'):
        candidate = root / label / 'candidate.o'
        current = Object(candidate).canonical()
        assert set(baseline[0]) == set(current[0])
        changed = [n for n in baseline[0] if baseline[0][n] != current[0][n]]
        assert changed == ['EchoCanceler'], changed
        assert baseline[1:] == current[1:], 'data, bindings or resolved-consumer drift'
        assert named_data(before) == named_data(candidate)
        assert {n:v for n,v in allocated(before).items() if n != '.text'} == {
            n:v for n,v in allocated(candidate).items() if n != '.text'}
        records[label] = {'emitted_bodies': len(current[0]),
                          'changed_bodies': changed,
                          'unchanged_bystanders': len(current[0])-1,
                          'data_bindings_consumers_equal': True}
    mu_first = root/'product-inside-mu-first/candidate.o'
    consumer_control = root/'product-inside-mu-first-history-first/candidate.o'
    assert mu_first.read_bytes() == consumer_control.read_bytes(), 'consumer operand control changed emission'
    # Inspect the actual emitted RTL, rather than the optimizer's value tables.
    base = function_chunk(root/'baseline/Fdspkrnl.c.01.rtl', 'EchoCanceler')
    initial = function_chunk(root/'product-inside/Fdspkrnl.c.01.rtl', 'EchoCanceler')
    early = function_chunk(root/'product-inside/Fdspkrnl.c.07.addressof', 'EchoCanceler')
    gcse = function_chunk(root/'product-inside/Fdspkrnl.c.08.gcse', 'EchoCanceler')
    controls = {
        'baseline_initial_product_precedes_loop': base.index('(insn 173 ') < base.index('NOTE_INSN_LOOP_BEG', base.index('(insn 173 ')),
        'candidate_initial_product_inside_loop': initial.index('(note 175 ') < initial.index('(insn 183 '),
        'candidate_pre_gcse_product_inside_loop': early.index('(jump_insn 293 ') < early.index('(note 175 ') < early.index('(insn 183 '),
        'candidate_gcse_product_in_guarded_preheader': gcse.index('(jump_insn 293 ') < gcse.index('(insn 318 ') < gcse.index('(note 175 '),
    }
    assert all(controls.values()), controls
    assert '[ g ]' in base[base.index('(insn 173 '):base.index('(insn 173 ')+70]
    assert '[ g ]' in early[early.index('(insn 183 '):early.index('(insn 183 ')+75]
    product = gcse[gcse.index('(insn 318 '):gcse.index('(insn 318 ')+180]
    assert '(mult:SF (reg/v:SF 73 [ e ])' in product and '(reg/v:SF 66 [ mu ])' in product
    selected_label = next(label for label in ('product-inside-mu-first-history-first', 'product-inside-mu-first', 'product-inside')
                          if (root/label/'Fdspkrnl.c').read_bytes() == (d.ROOT/'src/service/Fdspkrnl.c').read_bytes())
    selected = root/selected_label/'Fdspkrnl.c'
    assert selected.read_bytes() == (d.ROOT/'src/service/Fdspkrnl.c').read_bytes(), 'actual source differs from selected cell'
    report = {'cells':6, 'collateral':records, 'stage_controls':controls,
              'selected_label': selected_label,
              'consumer_operand_control_raw_identical': True,
              'mu_first_instruction_control': 'Reference ae9b7/ae9b9 FLD ST0;FMULP ST,ST2 restored at candidate f7/f9; source e*mu uses nonpopping FMUL at f7.',
              'first_motion_stage':'08.gcse',
              'reference_boundary':{'empty_guard': '0xae9b3 cmp; 0xae9b5 jae',
                                    'product':'0xae9b7 fld; 0xae9b9 fmulp'},
              'selected_boundary':{'empty_guard':'0xf3 cmp; 0xf5 jae',
                                   'product': '0xf7 fld; 0xf9 fmulp' if selected_label.startswith('product-inside-mu-first') else '0xf7 fmul'},
              'limits':'Supported partial guard/product recovery; no exact function or runtime defect claimed. Stack allocation and multiply-consumer operand scheduling remain different.'}
    (root.parent/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('adapt guard audit: 6 source cells; 30 candidate body verdicts, 25 unchanged bystanders; 4/4 stage controls')


if __name__ == '__main__':
    d.REV = 'e98762ba1'
    d.OUT_NAME = 'echo-adapt-guard'
    d.SOURCE_PATHS = ('src/service/Fdspkrnl.c',)
    d.DUMP_FLAGS = ('-da',)
    d.variants = variants
    d.main()
    audit()
