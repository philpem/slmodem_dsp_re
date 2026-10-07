#!/usr/bin/env python3
"""Compile unchanged complete TUs for the bounded scratch-target investigation."""
import playbook_small_patterns as driver
import json

def manifest():
    rows = []
    for label, compiler, input_name in [('toneiir', 'cc1', 'toneiir.i'),
                                         ('FloatFIR', 'cc1plus', 'FloatFIR.ii'),
                                         ('v22', 'cc1', 'v22.i'),
                                         ('V27_SDM', 'cc1', 'V27_SDM.i'),
                                         ('V27_SDM-repeat', 'cc1', 'V27_SDM.i')]:
        family = label.removesuffix('-repeat')
        directory = 'build/gentoo-scratch-target-inputs/'+family+'/baseline'
        assert (driver.ROOT/directory/input_name).is_file()
        rows.append(dict(label=label, directory=directory, compiler=compiler, input=input_name))
    (driver.ROOT/'build/gentoo-scratch-target-manifest.json').write_text(json.dumps(rows, indent=2)+'\n')

if __name__ == '__main__':
    driver.REV = 'ca84f1cc'
    driver.OUT_NAME = 'gentoo-scratch-target-inputs'
    driver.SOURCE_PATHS = ('src/callprog/toneiir.c', 'src/dsp/FloatFIR.cpp',
                           'src/pump/v22/v22.c', 'src/fax/V27_SDM.c')
    driver.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    driver.variants = lambda path, source: {'baseline': source}
    driver.main()
    manifest()
