#!/usr/bin/env python3
"""Cross independently observed V22 control byte sampling lifetimes."""
import itertools
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V22FP_control')
    cells = {}
    for first, late, hdx in itertools.product((False, True), repeat=3):
        changed = fn
        declarations = []
        if first or late:
            declarations.append('\tunsigned char flags;')
        if hdx:
            declarations.append('\tunsigned char hdx_flags;')
        if declarations:
            changed = changed.replace('\n{\n', '\n{\n' + '\n'.join(declarations) + '\n\n', 1)
        if first:
            changed = changed.replace('\tfp->dsp->descrambler_on =',
                                      '\tflags = ctl->flags_0c;\n\tfp->dsp->descrambler_on =', 1)
            changed = changed.replace('(ctl->flags_0c >> 1)', '(flags >> 1)', 1)
            changed = changed.replace('= ctl->flags_0c & 1;', '= flags & 1;', 1)
        if hdx:
            changed = changed.replace('\tfp->dsp->scrambler_on =',
                                      '\thdx_flags = ctl->flags_0d;\n\tfp->dsp->scrambler_on =', 1)
            changed = changed.replace('if (ctl->flags_0d & V22_CTL_RETRAIN)',
                                      'if (hdx_flags & V22_CTL_RETRAIN)', 1)
        if late:
            changed = changed.replace('\tfp->dsp->r20 =',
                                      '\tflags = ctl->flags_0c;\n\tfp->dsp->r20 =', 1)
            changed = changed.replace('(ctl->flags_0c >> 2)', '(flags >> 2)', 1)
            changed = changed.replace('(ctl->flags_0c & V22_CTL_FREEZE_ADAPT)',
                                      '(flags & V22_CTL_FREEZE_ADAPT)', 1)
            changed = changed.replace('(ctl->flags_0c >> 7)', '(flags >> 7)', 1)
        label = 'baseline' if not (first or late or hdx) else 'capture-' + ''.join(str(int(x)) for x in (first, late, hdx))
        cells[label] = source[:start] + changed + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'd672e822'
    driver.OUT_NAME = 'playbook-v22-control-captures'
    driver.SOURCE_PATHS = ('src/pump/v22/v22stc.c',)
    driver.variants = variants
    driver.main()
