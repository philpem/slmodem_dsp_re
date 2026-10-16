#!/usr/bin/env python3
"""Test the original four literal CP payload-count captures, in the full TU."""
from pathlib import Path
import sys
import playbook_small_patterns as driver
from recovery_cp_crc import SOURCE
from recovery_cp_bound import variants as bound_variants

driver.REV = 'd9995470'
driver.SOURCE_PATHS = (SOURCE,)
driver.OUT_NAME = 'recovery-cp-captures'
driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')


def variants(path, source):
    source = bound_variants(path, source)['published-bound']
    old = ('\t\t\tfor (k = 0; k <= 3; k++) {\n'
           '\t\t\t\tt[k] = nof_58[k];\n'
           '\t\t\t\tn[k] = nof_58[k];\n'
           '\t\t\t}')
    new = '\n'.join('\t\t\tt[%d] = n[%d] = nof_58[%d];' % (k, k, k)
                    for k in range(4))
    assert path == SOURCE and source.count(old) == 1
    candidate = source.replace(old, new)
    assert candidate != source
    return {'baseline': source, 'four-captures': candidate}


driver.variants = variants
if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.main()
