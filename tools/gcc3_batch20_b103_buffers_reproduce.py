#!/usr/bin/env python3
"""Cross object-supported byte-buffer ownership and status boundaries."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    stable=(driver.ROOT/'build/gcc3-batch20-v90-b103-owner/b103/self-rate-owner-byte-print/b103.c').read_text()
    cells={'baseline':source}
    for tx,rx,byte,before in itertools.product((False,True),repeat=4):
        text=stable
        if tx:
            marker='\tif (self->tx_bits_wanted != 0) {'
            text=text.replace(marker,marker+'\n\t\tunsigned char *tx_buffer = (unsigned char *)self->tx_bits;',1)
            start=text.index(marker);end=text.index('\n\t} else {',start)
            block=text[start:end].replace('(unsigned char *)self->tx_bits,','tx_buffer,').replace('((unsigned char *)self->tx_bits)[i]','tx_buffer[i]')
            text=text[:start]+block+text[end:]
        if rx:
            marker='\tif (self->tx_bits_wanted != 0 && n_rx != 0) {'
            text=text.replace(marker,marker+'\n\t\tunsigned char *rx_buffer = (unsigned char *)self->rx_bits;',1)
            start=text.index(marker);end=text.index('\n\t}',start)
            block=text[start:end].replace('((unsigned char *)self->rx_bits)[i]','rx_buffer[i]').replace('(unsigned char *)self->rx_bits,','rx_buffer,')
            text=text[:start]+block+text[end:]
        if byte:text=text.replace('\tstatus = result & 0xff;','\tstatus = (unsigned char)result;')
        if before:
            marker='\tcase 7:\n';assignment='\t\tresult = DPSTAT_CONNECT;\n'
            assert text.count(assignment)==1;text=text.replace(assignment,'').replace(marker,marker+assignment)
        cells['-'.join(n for n,v in [('tx-buffer',tx),('rx-buffer',rx),('byte-status',byte),('connect-before-print',before)] if v) or 'stable']=text
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-b103-buffers'
    driver.SOURCE_PATHS=('src/pump/b103/b103.c',);driver.variants=variants;driver.main()
