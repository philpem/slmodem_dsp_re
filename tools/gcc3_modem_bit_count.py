#!/usr/bin/env python3
"""Cross the imported int count contract with caller word normalization."""
import playbook_small_patterns as d


def variants(path, source):
    cells = {}
    assert source.count('unsigned short count);') == 2
    assert source.count('(unsigned short)n_tx);') == 1
    assert source.count('(unsigned short)n_rx);') == 1
    for wide in (False, True):
        for plain in (False, True):
            label = {(False,False):'baseline',(False,True):'calls',
                     (True,False):'declaration',(True,True):'both'}[wide,plain]
            text = source
            if wide:
                text = text.replace('unsigned short count);','int count);')
            if plain:
                text = text.replace('(unsigned short)n_tx);','n_tx);')
                text = text.replace('(unsigned short)n_rx);','n_rx);')
            cells[label] = text
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '9fabd432'
    d.SOURCE_PATHS = ('src/pump/b103/b103.c','src/pump/v23/v23.c')
    d.OUT_NAME = 'gcc3-modem-bit-count'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.main()
