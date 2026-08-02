# The V.34 fast pass

**Applies to tasks #33 and #35–#45. Deferred work is task #47.**

Phase 6 is ~110 KB of code — `v34handshak` alone is 61 KB across 87 states.
At the pace phases 1–5 were done it is many sessions. This is the agreed
change of sequencing, decided after V.34's first 4.5 KB.

It is **sequencing, not a drop in quality**. Nothing that establishes
correctness is skipped.

## What every module still gets

1. Read from the disassembly, not from a summary of it.
2. Written in C.
3. **A tier-1 differential test against the blob.** Non-negotiable, and not
   overhead: it is what makes writing fast. `V34TimingFilter` took three
   attempts and each fault was localised in one run. Without it there is no
   way to know the code is right and no way to find out later.
4. A one-line entry in `docs/deviations.md` for anything that looks wrong,
   marked `unmeasured`.
5. Brief structural comments — what the object is, what the fields are, what
   the loops do.

## What is deferred to #47

- **Coefficient derivations.** Tracing a table to its closed form serves
  *regeneration* at a different sample rate. Until the 8 kHz retarget, a
  byte-exact copy is byte-exact and the differential test proves it with no
  derivation at all.
- **Reachability measurement** for deviations. The entry is cheap; measuring
  whether it fires is not.
- **File-header rationale** and findings prose.

## Why the one-line deviation entries are not deferred

An unrecorded observation is unrecoverable — it will not be noticed twice.
D28 is the precedent: entered cheaply, later measured, then retracted. That
is the process working, and it only works if the entry exists.

## The one rule that is not relaxed

**Nothing is committed that has not passed a differential test**, and nothing
is committed that is wrong-but-plausible. If a function cannot be made to
pass, it is left out and the attempt is recorded. Two functions were reverted
under exactly this rule during the first V.34 pass.

Run `make phase`, not `make test`.
