# Commenting pass — plan and ledger

The reconstruction (`docs/remaining.md`) and the field-naming phase
(`docs/fieldnaming.md`) are both complete. This is the phase after: the
existing commentary is written in reverse-engineering investigation voice —
bare object addresses, opcode-level derivation trails, "nothing here proves
that" hedging — which was the right voice for *deriving* the code and is the
wrong voice for *maintaining* it. This phase moves the tree toward comments
that explain what the code does and why, for a human who did not do the
reverse engineering and does not need to.

**This is a pure documentation pass.** Like field naming, a comment-only
change cannot move generated code, so the only regression this phase can
cause is losing real information (an unresolved field's derivation, a real
behavioral caveat) or breaking a mutation-test anchor whose `find`/`replace`
text depends on an exact comment string. Every wave gates on:
- `tools/toolchain/byteident.py --ratchet` (`make byteident-ratchet`) — must
  stay at the current floor (736/1852 EXACT, 796/1852 grade 0-or-1). A
  comment change can never legitimately move this; any drop is a mistake to
  find, not a tolerance to accept.
- `python3 tools/anchorcheck.py` clean, whenever a touched file has any
  mutation suite against it.
- The relevant `make one` tests for anything whose *meaning* was
  reinterpreted while rewriting (rare — this is a comment pass, not a
  naming pass — but a stale/wrong old comment being corrected counts).

## The four comment categories, and what happens to each

1. **Thin bare-address comments** (`/* 0x62ac5 */`) — reconstruction
   apparatus, zero maintainer value. Delete outright where free; where the
   exact text is a mutation-anchor disambiguator (`tools/cmtsites.py`'s
   "frozen" classification), either add real prose as a separate block
   nearby (task #65's proven pattern, `docs/findings.md` F1180) or edit the
   anchor's `find`/`replace` in `test/mutations/*.json` to a different
   disambiguator (context lines instead of the address) and re-verify with
   `anchorcheck.py` before deleting the comment. Anchors only ever target
   `.c`/`.cpp` files (confirmed: `grep '"file".*"include/"' test/mutations/
   *.json` — zero hits), so this only matters for the later `.c`-file pass,
   never for headers.
2. **Inline R/E narration explaining control flow** — real content, wrong
   voice and an address nobody but the reconstruction needed. Rewritten in
   place, in plain English, as a normal engineering comment.
3. **Struct field derivation trails** — CLAUDE.md's own naming rule says a
   field that is bounded but not established keeps its derivation in the
   comment on purpose; that is not reconstruction noise, it is why the field
   isn't more confidently named yet, and stays. A field that already has a
   **real name** gets its trail compressed to one line plus a finding-number
   citation (the full trail already lives in `docs/findings.md`), not
   deleted outright — the reasoning stays retrievable, just not repeated
   inline.
4. **Function-purpose block comments** — often already most of the way
   there, just in investigation voice with hedging ("nothing here proves
   that"). Reformatted into Doxygen `@brief`/`@param`/`@return`, confidence
   language cleaned up where the derivation is actually solid, hedging kept
   where the tree's own uncertainty is real (do not manufacture false
   confidence — an honestly-uncertain comment is still better documentation
   than a confidently wrong one).

## Format: Doxygen-style, convention only

`/** @brief ... @param ... @return ... */`. No syntax alternative was worth
considering — `clang-doc`, Sphinx (via Breathe/Exhale) and every
`clangd`-based IDE tooltip either are Doxygen or consume Doxygen-style
blocks natively, so the ecosystem has converged on this as the interchange
format regardless of whether anything actually runs Doxygen. **Decision:**
adopt the convention now; do not stand up a `Doxyfile` or a generated docs
site this round — a small, separable follow-up once the comments themselves
are in shape.

## Decisions made 2026-09-04

- **Scope for the first wave: `include/dsplib/*.h` only** (222 files, ~1,233
  function-like declarations by a rough grep). Full `@brief`/`@param`/
  `@return` treatment. Static/internal helper functions inside `.c`/`.cpp`
  files are a later, separate pass — smaller first wave, faster to review.
- **The 941 anchor-frozen R/E comments (19 files under `test/mutations/`)
  get their anchors updated now**, not deferred — when that later `.c`-file
  pass reaches them, the mutation JSON's `find`/`replace` gets a new
  disambiguator (surrounding code context) and `anchorcheck.py` re-verifies
  uniqueness, so nothing here becomes a permanent R/E fossil. Not relevant
  to the header-only first wave (headers carry no mutation anchors at all).
- **No Doxygen build this round** — convention only, per above.

## Known tooling issue

`tools/cmtsites.py` hangs pathologically on `src/fax/v17cfg.c` (observed:
6+ minutes at 100% CPU, killed rather than waited out). Not required for
the header-only first wave (headers carry no mutation anchors), but must be
fixed or routed around before the later `.c`-file pass, which needs it for
every anchor-adjacent file.

## Baseline measurement, 2026-09-04

```
comment-bearing lines, src/+include/ (excl. re/)   16,054
total lines of code, src/+include/                 211,560
header files, include/dsplib/                      222
function-like declarations in headers (rough)      1,233
existing Doxygen-style comments                     0 (one accidental match)
```

Header files by family (for wave splitting):

```
V90*.h            36
V92*.h            20
v34*.h            13
VPcm*.h            1
fax/v8/v17/v27/v29/class1*.h   19
everything else (misc services, DSP primitives, callprog)  133
```

## Sequencing versus the byte-identity pass (decided 2026-09-04)

The user asked, correctly: if the `.c`-file phase of this pass edits
mutation anchors and rewrites function-body comments, and the byte-identity
improvement pass (queued next, `docs/fieldnaming.md`'s closing "next steps")
restructures those same function bodies for codegen matching, doing the
comment work first means documenting code that is about to change shape,
and touching 19 files' anchors twice.

**Resolution:** the two phases don't actually overlap where it matters.
- **Headers (`include/dsplib/*.h`) carry no mutation anchors at all**
  (confirmed by grep, see above) and are declarations/struct layout only —
  byte-identity work never touches them. The header-only phase of this pass
  is safe in EITHER order and proceeds now.
- **The `.c`-file phase** (thin-comment deletion, R/E narration rewrite,
  anchor updates) is deferred until AFTER the byte-identity pass, so
  function bodies get restructured once and documented once, not
  documented-then-disrupted-then-redocumented.

## Wave 1 (pilot) — v34*.h cluster

Launched 2026-09-04 as a deliberately small, well-understood pilot (13
files) to validate the methodology's actual output quality before fanning
out to the larger V90/V92 clusters or the 133-file long tail. Scope:
`include/dsplib/v34*.h`. Header-only, so unaffected by the sequencing
decision above.

Status: done (commit d9d9d901). All 13 files converted: every function
declaration got a Doxygen block, resolved-name struct/macro fields had
their derivation trails compressed to a line plus a finding citation,
still-unresolved fields (`type_NNNN`/bare `fNNNN`/`pad_NNNN`) kept their
full trails untouched apart from voice, and two dead orphaned comment
blocks (describing a struct superseded by a later consolidation) were
removed from v34det.h. Along the way, one stale citation was caught and
fixed: a comment attributed to deviation D34, which was later retracted
(finding F122) -- worth a reminder for the next wave to check that a cited
finding or deviation is still live, not just that it exists.

`make check64`, `python3 tools/refcheck.py` and `python3 tools/onedef.py`
are clean. `make byteident-ratchet` was not run (no docker in this
environment) but a comment-only change cannot move it.

Open question for scaling this to V90/V92 and the long tail: several
long-but-not-evidentiary paragraphs (pure architecture/design rationale
with no hex to cut) do not fit category 3's "compress to one line"
instruction well -- compressing them would just delete information for
length. The next wave's brief should say explicitly that only the
*evidentiary* content (addresses, opcodes, byte-offset citations used as
proof) is what gets compressed, not narrative rationale that happens to be
long. Also: not every derivation trail names its finding number inline, so
budget time for a `docs/findings.md` grep per field before compressing.

## Confirmed three-step order (2026-09-04)

Keep scaling the header pass to completion first. Then run the
byte-identity improvement pass, working WITH the existing in-function
R/E comments left in place (their address/derivation content is exactly
the working material that pass needs). Only after that settles do the
`.c`-file phase of this commenting pass replace those in-function
comments — so the code is restructured once and documented once, per the
sequencing section above.

## Wave 2 — the remaining 209 headers, seven parallel clusters

Launched 2026-09-04, same methodology as the pilot, with the pilot's own
correction folded in: compress only EVIDENTIARY content (addresses,
opcodes, byte-offset citations used as proof) when a field has a real
name, never long-but-non-evidentiary architecture/design rationale --
that stays in full regardless of the field's naming state. Also budget a
`docs/findings.md` grep per field before compressing, since not every
derivation trail names its finding number inline.

| cluster | scope | files |
|---|---|--:|
| V90 A | `V90{AutoDigitalImpDetector,BitsToSymbol,CodecType,ConnectionEvaluator,ConstellationDesigner,ConstellationPower,CP,CPpck,CPUnPck,Demapper,Demodulator,DilDescriptorSettings,Equalizer,Jd,Mapper,MappingParams,Modem,Modulator,MP,Parameters}.h` | 20 |
| V90 B | `V90{Phase2Info,Phase3Demodulator,Phase3Modulator,Phase4Demodulator,Phase4Modulator,PreFilter,RDetector,Resampler,SdDetector,SessionFlag,SignBitsExtractor,SpectralConditions,SpectralShaper,SpectralShapingFilter,SpectralVerifier,TRN2Designer}.h` | 16 |
| V92/VPcm | `V92*.h`, `VPcmFloModem.h` | 21 |
| fax/v8/class1 | `class1*.h`, `fax*.h`, `v17*.h`, `v27*.h`, `v29*.h`, `v8*.h` | 19 |
| long-tail A | `Agc.h` through `fpm.h`/`fpm_iir.h` (alphabetical first third) | 45 |
| long-tail B | `fpm_mrf.h` through `toneiir.h` (alphabetical second third) | 39 |
| long-tail C | `v21cfg.h` through `x87copy.h` (alphabetical last third, mostly V.22/V.23/V.32) | 49 |

Status: running.
