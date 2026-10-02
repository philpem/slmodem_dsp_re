#!/usr/bin/env python3
"""Four complete-TU CD count/output lifetime and result-carrier controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FSE_decision_CD')
    old = '\t*mag = 0x3299;\n\treturn (unsigned short)((m->count & 1) ? 3 : 0);'
    assert fn.count(old) == 1
    cells = {'baseline': source}
    for label, declaration, assignment, result in (
            ('capture-count', 'int decision_count;', 'decision_count = m->count;',
             '(unsigned short)((decision_count & 1) ? 3 : 0)'),
            ('capture-int-result', 'int decision;', 'decision = (m->count & 1) ? 3 : 0;', 'decision'),
            ('capture-short-result', 'unsigned short decision;', 'decision = (m->count & 1) ? 3 : 0;', 'decision')):
        changed = fn.replace('\tshort n = (short)(m->count + 1);',
                             '\tshort n = (short)(m->count + 1);\n\t' + declaration)
        changed = changed.replace(old, '\t' + assignment + '\n\t*mag = 0x3299;\n\treturn ' + result + ';')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'b1632e79'
    driver.OUT_NAME = 'playbook-fse-cd-load'
    driver.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    driver.variants = variants
    driver.main()
