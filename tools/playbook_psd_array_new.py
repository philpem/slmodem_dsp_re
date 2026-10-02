#!/usr/bin/env python3
"""Five full-TU Psd primitive-array allocation/scaffold controls."""
import playbook_small_patterns as driver


def variants(path, source):
    cells = {'baseline': source}
    assert source.count('#include <math.h>') == 1
    scaffold = source.replace('#include <math.h>', '#include <math.h>\n#include <stddef.h>')
    old = 'inline void operator delete[](void *p) { sysdep_free(p); }'
    assert scaffold.count(old) == 1
    scaffold = scaffold.replace(old, old + '\nvoid *operator new[](size_t);')
    start = scaffold.index('\nPsd::Psd(') + 1
    end = scaffold.index('\n}\n', start) + 2
    allocator = '\n\ninline void *operator new[](size_t n)\n{\n\treturn sysdep_malloc(n);\n}'
    scaffold = scaffold[:end] + allocator + scaffold[end:]
    for label, first, second in [('raw-scaffold', False, False),
                                   ('first-new', True, False),
                                   ('second-new', False, True),
                                   ('both-new', True, True)]:
        text = scaffold
        if first:
            old = 'm_window = (float *)sysdep_malloc(length * sizeof(float));'
            assert text.count(old) == 1
            text = text.replace(old, 'm_window = new float[length];')
        if second:
            old = 'm_fft = (float *)sysdep_malloc((m_length + 1) * sizeof(float));'
            assert text.count(old) == 1
            text = text.replace(old, 'm_fft = new float[m_length + 1];')
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    driver.REV = '41b7a6fe'
    driver.OUT_NAME = 'playbook-psd-array-new'
    driver.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    driver.variants = variants
    driver.main()
