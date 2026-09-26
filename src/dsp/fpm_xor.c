/*
 * fpm_xor.c -- the FPM popcount table.
 *
 * Reconstructed from dsplibs.o:
 *
 *   FPM_xor_table   .rodata 0xc7a0   512 bytes, 256 shorts
 *
 * Entry i is the number of set bits in i, verified against the blob for all
 * 256 entries.  The name is the AUTHOR'S and describes the use, not the
 * content: consumers XOR two words and look the result up, so the entry is
 * the Hamming distance of the pair -- SGD_correlate and SGD_sequence_det
 * (src/fax/Sgd.c) do exactly that over symbol sequences.
 *
 * IT IS 256 ENTRIES AND ITS CONSUMERS INDEX IT WITH 16-BIT VALUES.  An XOR
 * whose high bytes differ reads past the table into whatever .rodata the
 * link put next -- there as here, but not the SAME next -- so every user
 * must keep its symbols' high bytes equal, and the SGD tests do.  (The
 * neighbour effect src/dsp/fpm_div.c describes -- the blob's FPM_div
 * underflow landing on FPM_xor_table[0] -- is about the BLOB's layout and
 * does not transfer to ours.)
 */

#include "dsplib/fpm.h"
