#!/usr/bin/env python3
"""Two bounded value-owner crosses with observed original use boundaries."""
import playbook_small_patterns as d


def variants(path, source):
    cells={'baseline':source}
    if path.endswith('V92bitsToSymbol.cpp'):
        a,z,fn=d.function(source,'V92BitsToSymbol::setSymbolsBlockSize')
        for parameter,early in ((0,0),(1,0),(0,1),(1,1)):
            done='done' if early else 'symbolsDone'
            limit='n' if parameter else 'symbolsBlockSize'
            body='V92BitsToSymbol::setSymbolsBlockSize(unsigned int n)\n{\n'
            if early:body+='\tunsigned int done = symbolsDone;\n\n'
            body+='\tsymbolsBlockSize = n;\n\tunsigned int bits = 0;\n\n'
            body+=f'\tif ({limit} > {done}) {{\n\t\tunsigned int left = {limit} - {done};\n\n'
            body+='\t\tif (left % V92BTOS_SYMBOLS_PER_FRAME != 0)\n\t\t\tbits = (left / V92BTOS_SYMBOLS_PER_FRAME + 1)\n\t\t\t       * bitsPerFrame;\n\t\telse\n\t\t\tbits = left * bitsPerFrame\n\t\t\t       / V92BTOS_SYMBOLS_PER_FRAME;\n\t}\n\n\treturn bits;\n}'
            cells[f'parameter-{parameter}-early-{early}']=source[:a]+body+source[z:]
    else:
        a,z,fn=d.function(source,'V92EchoCanceller::resetEchoHistory')
        old='\techoLength = echoDelay + (filterLength >> 1)\n\t\t     + (unsigned int)blk->V92_ECHO_DELAY_OFFSET;'
        assert fn.count(old)==1
        for direct,local in ((1,0),(0,1),(1,1)):
            text=fn
            if local:
                text=text.replace(old,old.replace('echoLength =','unsigned int length =')+'\n\techoLength = length;').replace('n < echoLength','n < length')
            if direct:text=text.replace('\tV92Parameters *blk = params;\n','').replace('blk->V92_ECHO_DELAY_OFFSET','params->V92_ECHO_DELAY_OFFSET')
            cells[f'direct-{direct}-local-{local}']=source[:a]+text+source[z:]
    assert len(set(cells.values()))==len(cells)
    return cells


if __name__=='__main__':
    d.REV='9e20a346';d.OUT_NAME='residual-value-owner'
    d.SOURCE_PATHS=('src/pump/v90/V92bitsToSymbol.cpp','src/pump/v90/V92EchoCanceller.cpp')
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
