/*
 * pow.S -- translation-unit / toolchain-input record representation, NOT
 * reconstructed content.
 *
 * The reference object is linked from an assembly input spelled `pow.S` that
 * the assembler records as four FILE entries -- `pow.S`, `<command line>`,
 * `<built-in>`, `pow.S` -- and it defines the eight local constants
 * `inf_zero`, `infinity`, `minf_mzero`, `minfinity`, `mzero`, `zero`, `one`
 * and `limit` in `.text` plus the global `pow` (`.text` 0xb0b80, 4,464 bytes
 * to the end of `.text`).  That is libm's x87 `pow()` statically linked into
 * the object, not code this project authored: `tools/coverage.py`'s NOT_OURS
 * set lists `pow` and its internals with "there is no reconstruction work here
 * to schedule, ever" (F1990).
 *
 * The owner's goal is that the partial link's input FILE set match the
 * original build's.  The four FILE records above are reproduced ONLY by a
 * `.S` input -- a `.c` emits neither `<command line>` nor `<built-in>` -- so
 * this file is that input, kept deliberately empty.  `ld -r` adds zero bytes
 * to every allocated section for it, and the FILE records are the whole
 * difference.  It is a file-set representation, not recovered content; the
 * libm body is out of scope and `pow` itself is supplied by the reference at
 * test-link time.  See F11407.
 */
