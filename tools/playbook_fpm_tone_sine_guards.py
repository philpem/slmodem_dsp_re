#!/usr/bin/env python3
"""Six full-TU signed-count boundary/sequential-guard controls."""
import hashlib, json, shlex, subprocess
import playbook_small_patterns as driver
from playbook_fpm_tone_sine import variants as generation_variants


def variants(path, source):
    source = generation_variants(path, source)['scale-lifetime-loop']
    start, end, fn = driver.function(source, 'FPM_TONE_generate')
    fn = fn.replace('\tint period = state->cfg.rev_period;\n', '')
    marker = '\telapsed = state->rev_count\n\t\t  + (count >> 3);'
    condition = 'if (period > 0 && period <= elapsed)'
    begin = fn.index('\n\tif (')
    middle = fn.index('\n\t} else {', begin)
    finish = fn.index('\n\n\tstate->phase', middle)
    flip = fn[begin + len('\n\t' + condition + ' {'):middle]
    assert flip.startswith('\n\t\tint phase')
    cells = {}
    for carrier in ('short-local', 'int-use', 'owner-field'):
        text = fn
        value = '(short)elapsed'
        if carrier == 'short-local':
            text = text.replace('\tint elapsed;', '\tshort elapsed;')
            value = 'elapsed'
        elif carrier == 'owner-field':
            text = text.replace('\tint elapsed;\n', '')
            assert text.count(marker) == 1
            text = text.replace(marker, '\tstate->rev_count = (short)(state->rev_count + (count >> 3));')
            value = '(short)state->rev_count'
        cond = 'state->cfg.rev_period > 0 && state->cfg.rev_period <= ' + value
        conjunction = text.replace(condition, 'if (' + cond + ')')
        if carrier == 'owner-field':
            conjunction = conjunction.replace('\t} else {\n\t\tstate->rev_count = (short)elapsed;\n\t}', '\t}')
        label = 'baseline' if carrier == 'short-local' else carrier + '-conjunction'
        cells[label] = source[:start] + conjunction + source[end:]
        begin = text.index('\n\tif (')
        finish = text.index('\n\n\tstate->phase', begin)
        nonrev = '' if carrier == 'owner-field' else '\tstate->rev_count = (short)elapsed;\n'
        sequential = ('\n\tif (state->cfg.rev_period > ' + value + ')\n\t\tgoto no_reversal;\n'
                      '\tif (state->cfg.rev_period <= 0)\n\t\tgoto no_reversal;\n\t{'
                      + flip + '\n\t}\n\tgoto generated;\nno_reversal:\n' + nonrev + 'generated:')
        text = text[:begin] + sequential + text[finish:]
        cells[carrier + '-sequential'] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    driver.REV = '54bf1179'
    driver.OUT_NAME = 'playbook-fpm-tone-sine-guards'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    out = driver.ROOT/'build'/driver.OUT_NAME
    seed = out/'fpm_tone/seed'; seed.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', driver.REV + ':src/dsp/fpm_tone.c'], cwd=driver.ROOT, text=True)
    seed_source = variants('src/dsp/fpm_tone.c', source)['baseline']
    (seed/'fpm_tone.c').write_text(seed_source)
    config = (driver.ROOT/'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/' + f if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    dst = '/work/fpm_tone/seed'
    command = driver.tc.docker_prefix(image, driver.ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + dst + ' && ' + driver.tc.compile_shell(driver.tc.GENTOO_COMPILER_PATH, flags + ['-da'], dst + '/candidate.o', dst + '/fpm_tone.c')]
    with (seed/'compile.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    raw = (seed/'candidate.o').read_bytes()
    (out/'fpm_tone/retained.o').write_bytes(raw)
    (seed/'provenance.json').write_text(json.dumps({'command': command, 'source_sha256': hashlib.sha256(seed_source.encode()).hexdigest(), 'object_sha256': hashlib.sha256(raw).hexdigest(), 'role': 'staged source seed, not production'}, indent=2) + '\n')
    driver.main()
