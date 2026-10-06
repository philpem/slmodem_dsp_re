#!/usr/bin/env python3
"""Test separately selected diagnostic counts and the original overflow arm."""
import playbook_small_patterns as d
import fse_getdiag_result_reproduce as prior
import playbook_fse_getdiag_guard as guarded


def candidate(source, early, positive):
    start, end, body = d.function(source, 'FSE_getdiag')
    if early:
        assert body.count('\t\tresult = n;\n') == 2
        body = body.replace('\t\tresult = n;\n', '')
        assert body.count('\t\tif (n > max)\n\t\t\tn = max;') == 2
        body = body.replace('\t\tif (n > max)\n\t\t\tn = max;',
                            '\t\tresult = n;\n\t\tif (n > max)\n\t\t\tresult = max;')
        body = body.replace('i < n;', 'i < result;').replace('if (n > 0)', 'if (result > 0)')
    if positive:
        old = '\t\tif (n > FPM_FSE_DIAG - 1) {\n\t\t\tstate->diag_n = 0;\n\t\t\tbreak;\n\t\t}\n'
        assert body.count(old) == 1
        left = body.index(old)
        right = body.index('\t\tbreak;', left + len(old))
        block = body[left+len(old):right]
        new = '\t\tif (n <= FPM_FSE_DIAG - 1) {\n' + ''.join('\t'+line+'\n' for line in block.splitlines())
        new += '\t\t} else {\n\t\t\tstate->diag_n = 0;\n\t\t}\n'
        body = body[:left] + new + body[right:]
    return source[:start] + body + source[end:]


def variants(path, source):
    control = prior.shared_result(guarded.variants(path, source)['positive-guard'])
    return {'baseline': source, 'result-guard-control': control,
            'early-selection': candidate(control, True, False),
            'positive-overflow-arm': candidate(control, False, True),
            'early-selection-positive': candidate(control, True, True)}


if __name__ == '__main__':
    d.REV = '6b4509bd'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'fse-getdiag-selection'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
