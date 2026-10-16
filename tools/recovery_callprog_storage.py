#!/usr/bin/env python3
"""Replay the original-offset-supported Callprog storage declaration order."""
import playbook_small_patterns as driver


def variants(path, source):
    first = source.index('static unsigned char next_state_due_cptd[')
    last = source.index('\n\n', first)
    declarations = source[first:last].splitlines()
    assert len(declarations) == 11
    names = ('next_state_due_cptd', 'next_state_due_line_clear_timeout',
             'enable_line_clear_timeout', 'message_due_cptd',
             'message_due_line_clear_timeout', 'next_state_due_timeout',
             'message_due_timeout', 'timeout_table', 'automode_table',
             'toneiir_dialtone_table', 'toneiir_busy_table')
    ordered = [next(line for line in declarations if ' ' + name + '[' in line) for name in names]
    assert len(set(ordered)) == 11
    return {'baseline': source,
            'original-storage-order': source[:first] + '\n'.join(ordered) + source[last:]}


if __name__ == '__main__':
    driver.REV = '4d2efa2d'
    driver.SOURCE_PATHS = ('src/callprog/Callprog.c',)
    driver.OUT_NAME = 'recovery-callprog-storage'
    driver.DUMP_FLAGS = ('-v', '-save-temps')
    driver.variants = variants
    driver.main()
