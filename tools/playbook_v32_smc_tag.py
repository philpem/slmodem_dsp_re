#!/usr/bin/env python3
"""Six full-TU shifted-tag width/absolute-traversal controls."""
import playbook_small_patterns as driver
from playbook_v32_smc_abs import variants as traversal_variants


def narrow_tag(source, name):
    start, end, fn = driver.function(source, name)
    rhs = 'mode << 8' if name == 'SMCv32_encoder_dif' else 'smc->mode << 8'
    old = 'const int tag = ' + rhs + ';'
    assert fn.count(old) == 1
    fn = fn.replace(old, 'const short tag = (short)(' + rhs + ');')
    return source[:start] + fn + source[end:]


def variants(path, source):
    cells = {}
    parents = {'retained': source, 'traversal': traversal_variants(path, source)['both']}
    for traversal, parent in parents.items():
        forms = {'int-tag': parent, 'abs-short-tag': narrow_tag(parent, 'SMCv32_encoder_abs')}
        coherent = parent
        for name in ('SMCv32_encoder_dif', 'SMCv32_encoder_abs', 'SMCv32_encoder_tcm'):
            coherent = narrow_tag(coherent, name)
        forms['all-short-tags'] = coherent
        for tag, text in forms.items():
            label = 'baseline' if traversal == 'retained' and tag == 'int-tag' else traversal + '-' + tag
            cells[label] = text
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    driver.REV = '6eff516d'
    driver.OUT_NAME = 'playbook-v32-smc-tag'
    driver.SOURCE_PATHS = ('src/pump/v32/V32SMC_TX.c',)
    driver.variants = variants
    driver.main()
