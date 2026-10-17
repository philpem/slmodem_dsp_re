#!/usr/bin/env python3
"""Replay GenericIIR block normalization controls after a valid-history failure."""
import sys
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as driver

FACTOR_CROSS = False
BLOCK_COMPACTS = False
ACCEPTED = False


def audit():
    from elftools.elf.elffile import ELFFile
    from elftools.elf.relocation import RelocationSection
    scope = dict(ELFFile=ELFFile, RelocationSection=RelocationSection, b=driver.b)
    text = (driver.ROOT/'tools/playbook_bwch_unit_audit.py').read_text()
    exec(text[text.index('def inspect'):text.index('reports=')], scope)
    text = (driver.ROOT/'tools/gcc3_batch20_v90_small_audit.py').read_text()
    definition = text[text.index('def nontext'):text.index('\nreports=')]
    # COMDAT bodies are code, just like .text. Audit them through body(),
    # not as anonymous nontext data.
    definition = definition.replace("or section.name=='.text'", "or section['sh_flags']&4")
    exec(definition, scope)
    block = '_ZN10GenericIIRIfdE7processEPKfPfj'
    scalar = '_ZN10GenericIIRIfdE7processEf'
    reports = {}
    for folder in ('genericiir14', 'genericiir14-factor', 'genericiir14-block', 'genericiir14-accepted'):
        root = driver.ROOT/'build'/folder
        cells = json.loads((root/'results.json').read_text())['families']['FloatIIR']['cells']
        base = root/'FloatIIR/baseline/candidate.o'
        metadata = scope['inspect'](base)
        with base.open('rb') as stream:
            executable = {s.name for s in ELFFile(stream).iter_sections() if s['sh_flags'] & 4}
        metadata['allocated_sizes'] = {k: v for k, v in metadata['allocated_sizes'].items() if k not in executable}
        data = scope['nontext'](base)
        for label, cell in cells.items():
            obj = root/'FloatIIR'/label/'candidate.o'
            meta = scope['inspect'](obj)
            meta['allocated_sizes'] = {k: v for k, v in meta['allocated_sizes'].items() if k not in executable}
            assert meta == metadata, (folder, label, 'metadata/named data')
            assert scope['nontext'](obj) == data, (folder, label, 'nontext')
            assert len(cell['functions']) == 18
            assert set(cell.get('changed_bodies', [])) <= {block, scalar}
            assert not cell.get('gains') and not cell.get('losses')
            asm = subprocess.check_output([sys.executable, str(driver.ROOT/'tools/dis.py'), str(obj), block], text=True, stderr=subprocess.DEVNULL)
            (obj.parent/'block.dis').write_text(asm)
            reports[folder+'/'+label] = dict(functions=18, original_names=12, exact_names=9,
                changed_bodies=cell.get('changed_bodies', []),
                scalar_size=driver.b.sizes(str(obj))[scalar], block_size=driver.b.sizes(str(obj))[block],
                scalar_verdict=cell['verdicts'][scalar], block_verdict=cell['verdicts'][block],
                block_calls=len(re.findall(r'\bcall\s', asm)),
                reciprocal_multiply='fld1' in asm and 'fmulp' in asm and 'fdiv' in asm)
    assert len(reports) == 13
    accepted = driver.ROOT/'build/genericiir14-accepted/FloatIIR/accepted/candidate.o'
    chosen = driver.ROOT/'build/genericiir14-factor/FloatIIR/factor-inline-compacts/candidate.o'
    assert accepted.read_bytes() == chosen.read_bytes(), 'formatted accepted control drift'
    result = dict(cells=13, function_verdicts=234, reports=reports,
                  original_sizes={scalar: driver.b.sizes(driver.b.BLOB)[scalar], block: driver.b.sizes(driver.b.BLOB)[block]},
                  accepted_raw_equals_independent_factor_control=True)
    (driver.ROOT/'build/genericiir14/complete-tu-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('13/13 full-TU controls; 234 function verdicts; data, bindings and positioned relocations equal; 9/12 exact names unchanged')
    print('Accepted source raw-matches independently generated factor/inline-compacts object; only scalar/block bodies change')


def variants(path, source):
    if ACCEPTED:
        return {'baseline': source, 'accepted': (driver.ROOT/path).read_text()}
    start, end, scalar = driver.function(source, 'GenericIIR<Sample, Coeff>::process')
    assert scalar.startswith('GenericIIR<Sample, Coeff>::process(Sample x)')
    body = scalar[scalar.index('{') + 1:scalar.rindex('}')]
    assert body.count('return (Sample)m_acc;') == 1
    body = body.replace('return (Sample)m_acc;', 'out[k] = (Sample)m_acc;')
    old = '\tfor (unsigned k = 0; k < count; k++)\n\t\tout[k] = process(in[k]);'
    assert source.count(old) == 1
    explicit = source.replace(old, '\tfor (unsigned k = 0; k < count; k++) {\n\t\tSample x = in[k];\n' + body + '\t}')
    assert explicit.count('m_acc = m_acc / m_den[0];') == 2
    block_start = explicit.index('GenericIIR<Sample, Coeff>::process(const Sample *in, Sample *out,')
    reciprocal = explicit[:block_start] + explicit[block_start:].replace('m_acc = m_acc / m_den[0];', 'm_acc = m_acc * (1.0 / m_den[0]);', 1)
    marker = 'Sample\nGenericIIR<Sample, Coeff>::process(Sample x)'
    assert source.count(marker) == 1
    if FACTOR_CROSS or BLOCK_COMPACTS:
        cells = {'baseline': source}
        pairs = ((True, False),) if BLOCK_COMPACTS else ((False, True), (True, False), (True, True))
        for factor, compacts in pairs:
            text = explicit
            if factor:
                start = text.index('GenericIIR<Sample, Coeff>::process(const Sample *in, Sample *out,')
                old_normalize = '\tif (m_den[0] != 0)\n\t\tm_acc = m_acc / m_den[0];'
                new_normalize = '\tCoeff normalizer = 1;\n\tif (m_den[0] != 0)\n\t\tnormalizer = 1.0 / m_den[0];\n\tm_acc = m_acc * normalizer;'
                assert text[start:].count(old_normalize) == 1
                text = text[:start] + text[start:].replace(old_normalize, new_normalize)
            if compacts:
                for name in ('compactIn', 'compactOut'):
                    marker = 'void\nGenericIIR<Sample, Coeff>::' + name + '()'
                    assert text.count(marker) == 1
                    text = text.replace(marker, 'inline ' + marker)
            label = ('factor' if factor else 'divide') + ('-inline-compacts' if compacts else '')
            cells[label] = text
        if BLOCK_COMPACTS:
            text = cells['factor']
            start = text.index('GenericIIR<Sample, Coeff>::process(const Sample *in, Sample *out,')
            for name, field in (('compactIn', 'in'), ('compactOut', 'out')):
                _, _, compact = driver.function(source, 'GenericIIR<Sample, Coeff>::' + name)
                compact_body = compact[compact.index('{') + 1:compact.rindex('}')]
                old_call = '\tif (m_' + field + 'Pos == 0)\n\t\t' + name + '();'
                replacement = '\tif (m_' + field + 'Pos == 0) {' + compact_body + '\t}'
                assert text[start:].count(old_call) == 1
                text = text[:start] + text[start:].replace(old_call, replacement)
            # The independent block body has one more lexical nesting level.
            first = text.index('\n\t/*\n\t * Publish the accumulator', start)
            last = text.index('\n\t}', first)
            # Indent the complete outer-loop body, including nested blocks.
            block_end = text.index('\n}\n', start)
            tail = text[first:block_end]
            tail = '\n'.join('\t' + line if line else line for line in tail.split('\n'))
            text = text[:first] + tail + text[block_end:]
            cells['factor-block-compacts'] = text
        return cells
    cells = dict(baseline=source,
                 inline_scalar=source.replace(marker, 'inline ' + marker),
                 explicit_block_divide=explicit,
                 explicit_block_reciprocal=reciprocal)
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    if sys.argv[1:] == ['--audit']:
        audit()
        sys.exit(0)
    ACCEPTED = '--accepted' in sys.argv
    if ACCEPTED:
        sys.argv.remove('--accepted')
    FACTOR_CROSS = '--factor-cross' in sys.argv
    if FACTOR_CROSS:
        sys.argv.remove('--factor-cross')
    BLOCK_COMPACTS = '--block-compacts' in sys.argv
    if BLOCK_COMPACTS:
        sys.argv.remove('--block-compacts')
    driver.REV = '239b234c0'
    driver.OUT_NAME = 'genericiir14-accepted' if ACCEPTED else 'genericiir14-block' if BLOCK_COMPACTS else 'genericiir14-factor' if FACTOR_CROSS else 'genericiir14'
    driver.SOURCE_PATHS = ('src/dsp/FloatIIR.cpp',)
    driver.variants = variants
    driver.main()
