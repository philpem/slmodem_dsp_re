#!/usr/bin/env python3
"""Two witnessed terminal-control crosses on complete Gentoo TUs."""
import playbook_small_patterns as d


def variants(path, source):
    cells = {}
    if path.endswith('V90Modem.cpp'):
        a, z, fn = d.function(source, 'V90Modem::setSessionFlag')
        marker = '\tif (which == 0)'
        prefix = fn[:fn.index(marker)]
        assert prefix.count('\tint which = side;\n\n') == 1
        for direct, switch in [(0, 0), (1, 0), (0, 1), (1, 1)]:
            head = prefix.replace('\tint which = side;\n\n', '') if direct else prefix
            value = 'side' if direct else 'which'
            if switch:
                body = head+('\tswitch ('+value+') {\n\tcase 0:\n'
                    '\t\tmodulator->setSessionFlag(flag);\n\t\treturn;\n'
                    '\tcase 1:\n\t\tdemodulator->setSessionFlag(flag);\n\t\tbreak;\n\t}\n}')
            else:
                body = fn.replace('\tint which = side;\n\n', '').replace('which ==', 'side ==') if direct else fn
            cells['baseline' if not (direct or switch) else f'direct-{direct}-switch-{switch}'] = source[:a]+body+source[z:]
    else:
        a, z, fn = d.function(source, 'DialerAbort')
        marker = '\n\t/*\n\t * Written as one condition'
        split = fn.index(marker)
        head = fn[:split]
        rest = fn[split:fn.rindex('\n}')]
        assert head.count('\n\t\treturn;\n\t}') == 1
        for unsigned, common in [(0, 0), (1, 0), (0, 1), (1, 1)]:
            first = head.replace('d->progress_state > 10', '(unsigned)d->progress_state > 10u') if unsigned else head
            if common:
                first = first.replace('\n\t\treturn;\n\t}', '\n\t} else {')
                body = first+'\n'+rest+'\n\t}\n}'
            else:
                body = first+rest+'\n}'
            cells['baseline' if not (unsigned or common) else f'unsigned-{unsigned}-common-{common}'] = source[:a]+body+source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '511a7c14'
    d.OUT_NAME = 'small-call-result'
    d.SOURCE_PATHS = ('src/pump/v90/V90Modem.cpp', 'src/dialer/Dialer.c')
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
