# Code-style / fidelity review — plan and ledger

Requested 2026-09-04 by the project owner: "would a human write code like
this?" A five-agent parallel investigation (typecasts/void* signatures,
the `asm()`-label symbol override, line-wrapping, magic numbers/struct-
to-byte-pointer casts, and an open-ended sweep) answered the owner's seven
specific concerns plus found one more. Full findings are in the session
transcript; this file tracks the resulting plan and its execution.

## Verdicts on the seven original concerns

1. **Typecasts** — real concern, but 8,209 casts tree-wide is too large for
   manual triage and grep can't separate real narrowing casts from
   redundant ones reliably. Needs a proper tool (clang-tidy). See "Tier 3"
   below.
2. **void\* passed to internal functions** — 180 sites tree-wide, but 75%+
   are FORCED: genuine function-pointer dispatch tables in the fax cluster
   (`vxx_create[13]` etc. in `faxvmi.h`) that must share one signature.
   `descrambleGPA`/`descrambleGPC`, the owner's own seed example, turned
   out to be an open, already-documented signature-ambiguity question
   (`v34shell.h` keeps two candidate typedefs side by side because which
   one the object's `scramble` field really is isn't settled) -- not a
   style bug. ~45 non-table sites are real candidates, need per-site
   table-membership verification. **The owner asked this specific fact be
   settled, and flagged there may be other similar cases** -- see
   "Settling open signature questions" below.
3. **`VPcmXfCreate.cpp`'s `asm("_ZN...")` override** — necessary,
   documented apparatus (finding F1340): GCC 3.4's placement-`new` inserts
   a null-check the blob's actual instructions don't have (unconditional
   construct, check only afterward), so this device forces a direct call
   to the real constructor symbol instead. Tree-wide: 50 sites, 17 files,
   only the first cites F1340. **The owner pushed back on this as a
   worse-than-necessary hack and asked whether a compiler flag can
   suppress the null-check instead** -- see "Investigating -fno-check-new"
   below; this reopens whether Tier 1's "add missing citations" fix is
   even the right fix, or whether the whole device should be replaced.
4. **Line wrapping** — confirmed, real, safe. 790 of 1,243 short-wrapped
   string-literal pairs tree-wide, spanning the entire project's history.
   Zero byte-identity risk (string concatenation is preprocessor-level).
5. **Unnamed fields** — 625 unnamed identifiers tree-wide as of
   2026-09-04 (358 `type_NNNN` family, 89 bare `fNNNN`, plus 64 live
   `pad_NNNN` already tracked by the pad-audit). This is "continue the
   field-naming phase" with its own proven wave methodology.
6. **Magic numbers** — real but mixed. `v34scram_tiles` (the owner's own
   example) is actually this project's C89 static-assert idiom, not a
   magic number. V.34 runs 2.6-3.6x denser in bare numeric literals than
   later clusters, supporting "worse in the early stuff." Highest-value
   slice: unnamed flag-bit tests (`x & 0x0008`, no name) where CLAUDE.md's
   own rule already says to name them.
7. **Struct-to-byte-pointer casts, `v34pcmif.c`** — two different things
   share the same shape. Casting to a real, already-modeled substruct via
   an offset macro (e.g. `struct v34_ratecfg`) is this project's actual
   architecture for a struct built up incrementally across files -- not a
   bug. Raw triple-indirect hex-offset chasing through the untyped
   `void *p3548` bridge into the C++ side IS the real anti-pattern, and
   it's undocumented. Needs a proper `extern "C"` accessor exposed from
   the C++ side, not a mechanical rename. `v34pcmif.c` was never actually
   in scope for any field-naming wave -- not a regression, just never
   covered.

## Found beyond the seven

**`src/fax/class1tx.c` reinterprets `struct fax_class1 *ctx` as a raw
`unsigned short *` scratch buffer 31 times**, writing real data through it
in at least one state (`_tx_scrambled_ones_state`). Traces to
`fax_class1`'s still-unmodelled 4,091-byte `pad_005` floor -- every call
site passes the whole context and lets the callee index into unmodelled
space rather than take the address of a real field, because none exists
yet. Same pattern as concern 7, generalized, and a precise marker of
exactly which struct needs real fields next. Fix is zero-byte-identity-risk
once the field exists (same address), but the field has to exist first --
scoped Tier 2 work, not a one-line fix.

## Reassurance

Broad sampling across V.34/V.90/fax/service code, multiple eras, found the
tree reads as genuinely careful, well-cited human-quality engineering, not
systemically "decompiler output wearing a costume." No comment/code
correctness disagreements found in sampled files (weak evidence, small
sample). Deep nesting in DSP/state-machine code looked justified by real
branch structure, not gratuitous.

## Project decision, recorded for later (owner, 2026-09-04)

**Long-term plan, once the reconstruction is fully done: relax the
requirement for structures to match the blob's exact layout, and remove
the field-alignment/offset assertions** (`*_ASSERT_OFF`, `*_OFF` macros,
`V34SCRAM_ASSERT` and siblings) -- since the object will no longer be used
as the runtime reference once this tree replaces it. **Not actionable now**
-- the whole differential-test methodology (`make period`, byte-identity
ratchet) depends on exact layout matching the blob for as long as the
blob is the oracle. Recorded here so it isn't lost; do not act on it while
`make period` is still the gate.

## Settling open signature questions

The owner asked to settle `descrambleGPA`/`descrambleGPC`'s real signature
specifically, and flagged there may be other similar cases. In progress --
see the wave below.

## Investigating -fno-check-new

The owner's instinct: an `asm()`-label symbol override that bypasses the
C++ type system to dodge a null-check is worse engineering than just
accepting the null-check GCC 3.4 would otherwise emit. Question: does
`-fno-check-new` (or an equivalent) let genuine placement-`new` syntax
produce the SAME assembly as the current `asm()` override -- i.e. was the
asm() device solving a problem a flag could have solved instead? In
progress -- see the wave below. Findings F1339-F1341 are the existing
record to check against before concluding this is a new idea.

## Tier 1 — mechanical, zero byte-identity risk

1. Join the 790 short-wrapped string-literal pairs to the tree's real
   79-80 column convention.
2. Add the missing F1340 citation as a one-line comment at the 49
   uncited `asm()`-override sites (pending the -fno-check-new
   investigation above -- may be superseded by a bigger fix).
3. Rename `class1tx.c`'s leftover positional params (`word3`/`word4`/
   `word7`/`word8`) -- confirmed unused, pure rename.

Status: launching.

## Tier 2 — real fixes, needs scoping, after the byte-identity pass

4. Model enough of `fax_class1::pad_005` to give `class1tx.c`'s
   ctx-as-buffer idiom a real field.
5. Build the `extern "C"` accessor(s) for `v34pcmif.c`'s `p3548`
   cross-boundary chase.
6. Sweep V.34 for unnamed flag-bit tests specifically and name the ones
   with existing evidence.
7. Verify the ~45 non-table `void*` internal signatures against actual
   table membership; retype whichever aren't forced.
8. Continue the field-naming phase on the 625 remaining unnamed
   identifiers.

Status: not started.

## Tier 3 — needs tooling

9. Run clang-tidy for redundant-cast detection across the 8,209 casts,
   plus a broader check set to see what else it surfaces.

Status: launching (clang-tidy installed by the owner, 2026-09-04).

## Do not touch

`descrambleGPA`/`GPC`'s typedef ambiguity (pending resolution above, but
not a style fix regardless), the `asm()` mechanism itself (pending the
-fno-check-new investigation, but necessary as things stand), the forced
fax-dispatch-table `void*` signatures, and the deep nesting in DSP/
state-machine code -- all confirmed necessary or already correctly
handled.
