#!/usr/bin/env python3
"""Diffcoder operand-order flip domain on the two defining period TUs.

Posted domain: #22 (diffcoder operand-order). Both ParallelDifferential*<h>::
process residuals are operand-ROLE swaps; the hypothesis is that the author's
source operand order is flipped relative to ours in both bodies, because our
GCC 3.4.2-r2 emits acc<-tree-second while the blob emits acc<-tree-first in
both. The flip is behavior-identical (commutative XOR), so no differential
change is expected; static checks only, no fuzzing or mutation execution.
"""
import playbook_small_patterns as driver

REV = '47174bf3157a98e54d5bb5d9a62b6e77bb8bc19d'
OUT_NAME = 'playbook-diffcoder-flip'
DUMP_FLAGS = ()
SOURCE_PATHS = ('src/pump/v90/V90SignBitsExtractor.cpp',
                'src/pump/v90/V90SpectralShaper.cpp')

DECODER_OLD = '\t\t*out = x ^ *state;'
DECODER_NEW = '\t\t*out = *state ^ x;'
ENCODER_OLD = '\t\tT x = *state ^ *in;'
ENCODER_NEW = '\t\tT x = *in ^ *state;'


def flipped(text, decoder, encoder):
    if decoder:
        assert text.count(DECODER_OLD) == 1
        text = text.replace(DECODER_OLD, DECODER_NEW)
    if encoder:
        assert text.count(ENCODER_OLD) == 1
        text = text.replace(ENCODER_OLD, ENCODER_NEW)
    return text


def variants(path, source):
    # The source files are unchanged in every cell; the domain varies the
    # shared header via HEADER_OVERLAYS. Four labels, one source text each.
    cells = {label: source for label in
             ('baseline', 'decoder-flip', 'encoder-flip', 'both-flip')}
    assert len(cells) == 4
    return cells


def header_overlays(path, label):
    if label == 'baseline':
        return {}
    import subprocess
    text = subprocess.check_output(
        ['git', 'show', REV + ':include/dsplib/DiffCoder.h'],
        cwd=driver.ROOT, text=True)
    return {'dsplib/DiffCoder.h': flipped(text,
                                          label in ('decoder-flip', 'both-flip'),
                                          label in ('encoder-flip', 'both-flip'))}


driver.REV = REV
driver.OUT_NAME = OUT_NAME
driver.DUMP_FLAGS = DUMP_FLAGS
driver.SOURCE_PATHS = SOURCE_PATHS
driver.variants = variants
driver.HEADER_OVERLAYS = header_overlays

if __name__ == '__main__':
    driver.main()
