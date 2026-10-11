#!/usr/bin/env python3
"""Cross original ADID group initialization and shared guarded-loop exits."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d

NAME = 'V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases'


def variants(path, source):
    a,z,fn = d.function(source, NAME)
    clear = '\tfor (i = 0; i < V90ADID_PHASES; i++)\n\t\tgroup[i] = ADID_NO_GROUP;'
    outer = '\t\tif (byte_280c[i] != 0 || group[i] != ADID_NO_GROUP)\n\t\t\tcontinue;'
    pool = '\t\tif (byte_280c[j] != 0 || group[j] != bestGroup)\n\t\t\tcontinue;'
    assert fn.count(clear) == fn.count(outer) == fn.count(pool) == 1
    cells = {'baseline': source}
    for late, grouping, pooling in itertools.product((False,True), repeat=3):
        if not (late or grouping or pooling):
            continue
        text = fn
        if late:
            text = text.replace('\tshort best = 0;', '\tshort best;').replace('\tshort count = 1;', '\tshort count;')
            text = text.replace(clear, clear+'\n\tcount = 1;\n\tbest = 0;')
        if grouping:
            text = text.replace(outer, '\t\tif (byte_280c[i] == 0 && group[i] == ADID_NO_GROUP) {')
            marker = '\t\t\tbestGroup = i;\n\t\t}\n\t}'
            assert text.count(marker) == 1
            text = text.replace(marker, '\t\t\tbestGroup = i;\n\t\t}\n\t\t}\n\t}')
        if pooling:
            text = text.replace(pool, '\t\tif (byte_280c[j] == 0 && group[j] == bestGroup) {')
            marker = '\t\ttotal = (short)(total + magnitudeCount[j][at]);\n\t}'
            assert text.count(marker) == 1
            text = text.replace(marker, '\t\ttotal = (short)(total + magnitudeCount[j][at]);\n\t\t}\n\t}')
        cells['late-%d-group-%d-pool-%d' % (late,grouping,pooling)] = source[:a]+text+source[z:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '860732a2'
    d.OUT_NAME = 'gcc3-adid-phase-grouping'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.DUMP_FLAGS = ('-dr',)
    d.variants = variants
    d.main()
