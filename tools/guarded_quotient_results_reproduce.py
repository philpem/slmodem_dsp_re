#!/usr/bin/env python3
"""Ordinary conditional assignments for three original guarded quotient fields."""
import playbook_small_patterns as d

def variants(path, source):
    if path.endswith('V90SignBitsExtractor.cpp'):
        old = '\tif (spacing_ != 0)\n\t\twidth = V90SBE_DECODER_SIZE / spacing_;\n\telse\n\t\twidth = 0;'
        new = '\twidth = spacing_ != 0 ? V90SBE_DECODER_SIZE / spacing_ : 0;'
    elif path.endswith('V90SpectralShaper.cpp'):
        old = '\tif (sr != 0)\n\t\tblockLength = 6 / sr;\n\telse\n\t\tblockLength = 0;'
        new = '\tblockLength = sr != 0 ? 6 / sr : 0;'
    else:
        old = '\tif (mp->shaperSR != 0)\n\t\tsignBitGroupSize = V90MAPPER_FRAME / mp->shaperSR;\n\telse\n\t\tsignBitGroupSize = 0;'
        new = '\tsignBitGroupSize = mp->shaperSR != 0 ? V90MAPPER_FRAME / mp->shaperSR : 0;'
    assert source.count(old) == 1
    return {'baseline': source, 'conditional-quotient': source.replace(old, new)}

if __name__ == '__main__':
    d.REV = '04eee73f'
    d.OUT_NAME = 'guarded-quotient-results'
    d.SOURCE_PATHS = ('src/pump/v90/V90SignBitsExtractor.cpp',
                      'src/pump/v90/V90SpectralShaper.cpp', 'src/pump/v90/V90Mapper.cpp')
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
