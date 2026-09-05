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

## Settling open signature questions — RESOLVED (F10154)

Not actually about `descrambleGPA`/`descrambleGPC` -- their signature was
never ambiguous (they install through the union's `put_bits` spelling,
3-arg, already matching exactly). The real ambiguity was
`scrambleGPA`/`scrambleGPC` against the union's `scramble` spelling, and
it was already settled with disassembly evidence (`movswl`/`cwtl` in the
object's own code) in `v34shell.c`'s own comment -- `v34shell.h`'s comment
had simply gone stale, describing `getFrame` as unreconstructed after it
no longer was. Fixed, comment-only (commit `cbd9cad8`). Five other
function-signature-ambiguity cases found tree-wide were checked and found
to be genuinely, correctly unresolved already (a stub whose object body
reads none of its arguments, so no evidence bounds true arity) -- no
action needed on any of them.

## Investigating -fno-check-new — RESOLVED, F1340 RETRACTED (F10155)

**The owner's instinct was right, and more completely than asked.**
`-fcheck-new` is off by default and was never in `tools/toolchain/
period.mk`'s `TC_FLAGS` -- F1340's reasoning (a `-nostdinc++` build forces
a user-declared placement form that GCC checks) was general-C++-rule
reasoning, never empirically tested, and the rule has a condition F1340
didn't check: the check only fires for a `throw()`-declared placement
`operator new`, not an ordinary non-throw one. Verified under the real
period compiler: a minimal non-throw placement `operator new` plus
genuine `new (self) Thing(...)` syntax, NO flag changes, reproduces the
blob's exact construct-then-check-later shape at `VPCMXF_Create`.
Declaring the same operator `throw()` reproduces the check precisely,
confirming the mechanism. **No build-flag change needed.** The fix is
source-level: one shared non-throw placement `operator new`/`operator
delete` pair, then replace each of the 50 `asm("_ZN...")` sites with
ordinary placement-new syntax -- at 49 of the 50 sites (everywhere except
`VPCMXF_Create` itself) this doesn't just match the blob, it *removes*
dead code the `asm()` device carries forward for no reason, a genuine
byte-identity IMPROVEMENT opportunity. Promoted to its own item under
Tier 2 (item 10, below) rather than Tier 1's "add a citation" fix, since
each site needs its own verification.

## Tier 1 — mechanical, zero byte-identity risk — DONE

1. **Done, merged.** 613 short-wrapped string-literal pairs joined across
   63 `src/` files (the 790 estimate undercounted -- real candidate count
   was ~1357 line-pairs, 613 joined, 613 correctly left wrapped as
   genuinely over the limit, 105 correctly excluded as brace-delimited
   initializer entries, not column-wrapped literals). 225 mutation-anchor
   strings updated across 40 `test/mutations/*.json` files.
2. ~~Add the missing F1340 citation~~ SUPERSEDED — F1340 is retracted
   (F10155); the whole `asm()` device is being replaced, not documented
   further. See Tier 2 item 10.
3. **Done, merged.** `class1tx.c`'s `_tx_scrambled_ones_state`: `word4`
   -> `src` (class-2 evidence), `word8` -> `tx_data_count` (class-1, the
   object's own "TxDatCnt" diagnostic string), `word3`/`word7` ->
   `unused_dst`/`unused_out_count` (confirmed dead in this function
   specifically). Same "some params named, some left as wordN" pattern
   found across every sibling in the `class1_state_fn` dispatch family --
   flagged for a future pass, not fixed here.

**Bonus, found while chasing Tier 1's verification runs**: `t_v90cdesign`'s
27-40 minute runtime (F10156) -- three of its own trials were accidentally
triggering `calcMtoMatchKtarget`'s own documented unbounded-loop hazard.
Fixed; `make period`'s full ~90-binary suite now runs in 3m42s, not
27-40+ minutes.

**Master gate, post-merge**: `make period` 374 passed, 0 failed, exit 0.
`make byteident-ratchet` unchanged at 736/1852 EXACT (39.7%), 796/1852
grade 0-or-1 (43.0%) -- zero codegen drift across all of Tier 1.

Status: DONE.

## Tier 2 — three sub-groups, different sequencing

Re-scoped 2026-09-05 once F10155 (asm() retraction) landed: items 9 and 10
turn out to BE the byte-identity improvement pass this session already
owed, not separate cleanup work waiting on it. Re-split accordingly.

**2a — byte-identity work, do next (this sweep, not deferred):**
9. `bugprone-narrowing-conversions` (439, clang-tidy) and
   `bugprone-incorrect-roundings` (43, clang-tidy) -- per-site `dis.py`
   verification against CLAUDE.md's forced-vs-free framework; only touch
   a site where the object's own code proves the current shape isn't
   forced.
10. **Replace the 50 `asm("_ZN...")` sites with genuine placement-`new`
    syntax** (F10155) -- declare a shared non-throw placement `operator
    new`/`operator delete`, convert each site, each gated on its own
    `byteident.py`/`make period` run. Real opportunity to IMPROVE the
    byte-identity count at 49 of the 50 sites.
7. ~~Verify the ~45 non-table `void*` internal signatures against actual
   table membership; retype whichever aren't forced~~ DONE, RESOLVED
   (F10158): all ~45 traced individually and all are FORCED -- opaque host
   handle (no real type exists, e.g. `pulse.c`), quoted external ABI
   contract (`cid.c`/`ringdet.h`/`vce.h` cite slmodemd's own declared
   prototypes verbatim), or a confirmed dispatch table (`dp_process_fn`,
   and a newly-confirmed one, `struct voice_config`'s `fn_04`/`fn_08`/
   `fn_0c`). Zero safe-to-retype sites, zero source changes. The suspected
   `voice.c` internal `RD_create`/`VOICE_create` dispatch does not exist
   (refuted -- those are pure external entry points). The `src/pump/v34/`
   `void *objp`-style "self" parameter (~110 more sites, out of this
   item's scope) looks like the same dispatch-table shape on a spot check
   but was not individually verified -- do not assume it is retypeable.

**2b — structural additions, scoped but bigger, after 2a:**
4. Model enough of `fax_class1::pad_005` to give `class1tx.c`'s
   ctx-as-buffer idiom a real field.
5. Build the `extern "C"` accessor(s) for `v34pcmif.c`'s `p3548`
   cross-boundary chase.

**2c — pure naming/comment work, safe any time, can run in parallel with
2a/2b since it mostly touches different files:**
6. Sweep V.34 for unnamed flag-bit tests specifically and name the ones
   with existing evidence.
8. Continue the field-naming phase on the 625 remaining unnamed
   identifiers.

**Sequencing note, still standing**: the `.c`-file phase of the
COMMENTING pass (thin bare-address comment removal, R/E-narration
rewrite, the 941 anchor-frozen mutation comments) still waits until AFTER
2a and 2b land, so function bodies get restructured once and documented
once -- unchanged from the original decision, just now grounded in a
concrete Tier 2 rather than an abstract "byte-identity pass."

Status: item 7 done (F10158, zero retypes -- see above); 9/10 still
pending; 2b/2c scoped, not started.

## PRIORITY — correctness, not style (class1rx.c null derefs) — RESOLVED

`clang-analyzer-core.NullDereference` found three sites in
`src/fax/class1rx.c` (lines 471, 495, 519): "access to field `link`
dereferences a null pointer loaded from field `vmi_b`." Investigated and
confirmed FALSE POSITIVE at all three -- `modem_vmi`/`vmi_b` are always
set together and cleared together across this function's own branches,
an invariant a function-local analyzer can't see. Checked the one way it
could theoretically break (a `sysdep_malloc` failure inside
`FAXVMI_create`) and confirmed it crashes earlier, inside `FAXVMI_create`
itself, matching this tree's documented convention that the object never
checks `sysdep_malloc`'s return value. Fixed with an explanatory comment
at the REINIT PATH block (commit `fd692952`), re-verified against
`t_class1initrx`/`t_class1delmodem` at unchanged check counts. No
correctness bug.

## Tier 3 — clang-tidy, DONE

Ran (compile_commands.json generated from the Makefile's real flags,
confirmed firing on a known-messy file before trusting the full run;
raw logs `/tmp/clangtidy_out/{redundant_all,broad_all}.log`, not
preserved past this session -- re-run to regenerate):

- **Redundant casts: 136 real sites** (not 8,209 -- that counted every
  cast including genuine narrowing/widening). Concentrated in
  `src/dsp/fpm_tone.c`, `Psd.cpp`, `toneiir.c`. Safe, mechanical,
  zero byte-identity risk (same-type cast is a compiler no-op).
- **Safe to act on**: redundant casts (136), `misc-const-correctness`
  (439, cosmetic), `bugprone-implicit-widening-of-multiplication-result`
  (90, adds a widening cast before multiply, doesn't change in-range
  results), every other `clang-analyzer-core.*` finding (~22 combined,
  small enough to fully triage in one sitting).
- **Needs `dis.py` verification per site**: `bugprone-narrowing-
  conversions` (439), `bugprone-incorrect-roundings` (43) -- folded into
  Tier 2 items 9 above.
- **Confirms scale, doesn't add new work**: `bugprone-casting-through-
  void` (4,073) is a much wider net over the same void*/cast concerns
  (points 1/2/7) already scoped above -- treat as a candidate list against
  the forced-vs-free classification already built, not as 4,073 new bugs.
- **Suppress as noise for this codebase**: `misc-use-anonymous-namespace`
  (142 -- flags deliberate file-scope `static` matching the blob's own
  measured linkage), `misc-new-delete-overloads` (20 -- the expected
  shape of the placement-construction apparatus, F1340/F7815/F10155),
  `misc-include-cleaner` (381), `bugprone-switch-missing-default-case`
  (44), `clang-analyzer-security.insecureAPI.*` (19, expected for 2005-era
  C). A `.clang-tidy` config disabling these is worth adding so a future
  run doesn't re-derive this.
- **Not yet triaged**: `clang-analyzer-optin.core.EnumCastOutOfRange`
  (113), `bugprone-too-small-loop-variable` (26, real-bug shape),
  `bugprone-easily-swappable-parameters` (282, a naming-weakness signal,
  not something to fix by reordering parameters).

## Do not touch

The `asm()` mechanism's UNDERLYING NEED is gone (F10155) but the 50 sites
themselves still work correctly as-is until Tier 2 item 10 replaces them
-- don't touch them piecemeal outside that workstream. The forced
fax-dispatch-table `void*` signatures, and the deep nesting in DSP/
state-machine code -- both confirmed necessary or already correctly
handled.
