# Reopening the rejected V8 CRC lead through RTL diagnostics

Issue [250](https://github.com/philpem/slmodem_dsp_re/issues/250), isolated
branch investigate/v8-crc-rtl-unblock, source baseline master9c0be89f.
PR245's active files are untouched. [Its F11672 record](https://github.com/philpem/slmodem_dsp_re/blob/17d182af9ff726e984bd14b1d419e667e518067f/docs/v8crc-v92cp-ctor-screen.md) remains on the other branch. Its eight sign-bit spelling
negatives remain valid; this experiment tests a new pass-derived ordering
hypothesis, not another extraction synonym.

## Object facts and pass boundary

Blob v8_crc is 41 bytes: unsigned memory load into EAX, signed register
extension AX→EDX, logical shift31, ADD EAX,EAX, then conditional XORs and
word store. Baseline is 40 bytes: unsigned load, register move, shift15.
Six predeclared controls compile the full baseline TU, the previously rejected
signed-field control and an extracted baseline function, each plain/-da.
All six compile, all three raw diagnostic pairs agree, full baseline
raw-reproduces production and extracted body matches the full TU.

Baseline expansion already has the blob's signed extension and shift31.
Those survive CSE2; combine deletes UID13's sign extension and changes UID14
to shift15. The standalone case repeats the same boundary. This is not a
register-allocation mismatch. GCC3 combine.c expands compound extension into
shifts and simplifies nested sign-bit extraction; the dump transition is the
measured evidence, not a claim that a source cast survived unchanged.

The signed-field negative (`int msb = hs->crc < 0 ? 1 : 0`, after unsigned
CRC read) preserves a shared HI load and both extensions through combine.
Regmove changes that HI load to a signed SI memory load and rewrites the
earlier unsigned consumer as a register re-narrowing. Thus the reversed
primary role appears at regmove, before local allocation or reload.

[GCC3 regmove.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/regmove.c)
`optimize_reg_copy_3` explains the constraint: only the extension carrying
the narrow source's death can be folded into its single memory definition;
intervening users are rewritten through subregs. This function runs in the
forward pass under `flag_expensive_optimizations`, even with `-fno-regmove`.
Four predeclared source-by-regmove controls confirm the trap: CRC bodies do
not change, both off cells lose exact v8_absfn and change V8agc. No flag adopted.

## New source prediction and exact result

The death-bearing extension is signed because it consumes the shared HI
value last. Compute the independent signed predicate first and read unsigned
CRC second: the unsigned extension should now carry the death and promote
the load unsigned. Three predeclared full-TU source cells test production,
the signed-field negative, and that single order reversal:

```c
int msb = hs->crc < 0 ? 1 : 0;
unsigned int crc = (unsigned short)hs->crc;
```

The first two raw-reproduce previous controls. The third changes the
regmove load to ZERO_EXTEND and leaves SIGN_EXTEND of its HI subreg followed
by shift31, as predicted. Its complete 41-byte v8_crc is EXACT under unchanged
flags. Sole changed function, zero exact losses, no neighboring definition
or field retyping. These are independent reads before any store. This is an
idiomatic evidence-supported representative, not unique original spelling.

## Reproduction and audit

At baseline9c0be89f, build `make tc`, archive `build/tc_out` as
`build/production-before`, then run:

```
python3 tools/gcc3_v8_crc_reproduce.py --baseline-dir build/production-before
python3 tools/gcc3_v8_crc_reproduce.py --regmove-cross --baseline-dir build/production-before
python3 tools/gcc3_v8_crc_reproduce.py --source-order --baseline-dir build/production-before
python3 tools/gcc3_v8_crc_audit.py
```

Tools fetch historical source from git and verify current header compatibility.
All commands/config/header/source/object hashes are recorded. Shared
experiment helpers append DSPLIB_REPRODUCE_BUGS after flags; Gentoo compiler
and its executed assembler identity print with the run. Domains were posted
before compilation in issue250 (initial body, comments5975070457 and5975084108).

13 compile cells,4 distinct sources (including extraction),6 distinct raw
objects,0 invalid compilations. Eleven full-TU cells have13 functions
and4 named data objects; all type/binding/visibility, named-data values,
allocated nontext/raw-data controls agree. This TU has no nontext relocations;
the audit refuses new ones. Two extracted cells are intentionally outside
that full-TU denominator. The balanced parser reads instruction patterns,
excluding metadata: 290/300 stage records parse, ten iterative GCSE dumps
refuse duplicate UIDs and remain explicitly unclassified. Five causal
controls fire: two combine, two signed promotion, one unsigned promotion.
Early audit plumbing failures (stdlib dis shadowing, missing import, iterative
GCSE duplication) were corrected or made explicit refusals before accepting
the audit; they are not compiler failures or clean-run evidence.

All300 fresh baseline objects raw-match the previously measured929 baseline.
After adoption, only V8global.c.o changes and raw-reproduces the exact candidate.
Whole-tree929→930/1852, exact bytes95782→95823, sole gain v8_crc, zero losses.
The first phase run fails refs because inherited F11692/F11693 headings used
a dash rather than the required period; punctuation is corrected here.
The first run still passes all388 period fixtures. Gate results are recorded
with the final run below. No fuzzing, mutation
execution, flag change, new reachability or full-profile completion claim.

## Transferable lever

For shared narrow loads with signed and unsigned consumers, trace expansion,
combine and regmove separately. A surviving extension can be folded backward
into the load according to the last consumer, independently of allocation.
A cast-only domain cannot determine independent source-read order. Conversely,
source order alone cannot resurrect an extension that combine already removed.
Use the surviving shared-load negative to derive the next ordering control.

Same-order partial link before/after: equal positioned bytes68908/943398,
exact relocations1028/18317 and exact symbols394/2907 unchanged; both
complete-object verdicts DIFFERENT. Existing CRC component passes22824 checks.

Final `make phase J=4` passes388/0 and the complete structural boundary;
285 suites/10038 static anchors remain attached, with0 problems. No anchor
change is needed. The final tools replay all13 cells and the audit passes.
