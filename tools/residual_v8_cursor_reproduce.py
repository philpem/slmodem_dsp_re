#!/usr/bin/env python3
"""Cross two independently observed per-sample cursor publications."""
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'V8Process')
    cells = {'baseline': source}
    for ring, symbol in ((1, 0), (0, 1), (1, 1)):
        text = fn
        if ring:
            old = '\t\t*out++ = *v->tx_ring_base++;\n\t\tif (v->tx_ring_base >= v->tx_ring + V8_TX_RING_END)\n\t\t\tv->tx_ring_base = v->tx_ring;'
            assert text.count(old) == 1
            text = text.replace(old, '\t\tshort *tx = v->tx_ring_base;\n\t\t*out++ = *tx++;\n\t\tif (tx >= v->tx_ring + V8_TX_RING_END)\n\t\t\ttx = v->tx_ring;\n\t\tv->tx_ring_base = tx;')
        if symbol:
            old = '\t\tv->tx_sym_b[0] = (short)c;\n\t\tv->tx_sym_b[1] = 0;\n\t\tv->tx_sym_b += 2;\n\t\tif (v->tx_sym_b >= v->tx_symbols + V8_TX_SYMBOLS)\n\t\t\tv->tx_sym_b = v->tx_symbols;'
            assert text.count(old) == 1
            text = text.replace(old, '\t\tshort *sym = v->tx_sym_b;\n\t\tsym[0] = (short)c;\n\t\tsym[1] = 0;\n\t\tsym += 2;\n\t\tif (sym >= v->tx_symbols + V8_TX_SYMBOLS)\n\t\t\tsym = v->tx_symbols;\n\t\tv->tx_sym_b = sym;')
        cells[f'ring-{ring}-symbol-{symbol}'] = source[:a] + text + source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = '153b22b0'
    d.OUT_NAME = 'residual-v8-cursor'
    d.SOURCE_PATHS = ('src/v8/V8Interface.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
