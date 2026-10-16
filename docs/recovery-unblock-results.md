# Delegated byte-exact unblocking results

Branch `investigate/recovery-unblock`, based on `426814ea`.
Three Sol agents executed the bounded domains in
[the plan](recovery-unblock-plan.md); the parent reviewed and integrated them.
The CID agent independently reviewed both proof extensions with additional
negative probes. The integrated implementation checkpoint is `6786ac01`.

**Result: two additional exact functions, zero exact losses.** The fresh
canonical census is 1,093/1,852 exact names and 123,672/720,125 original bytes,
up by two names and 957 bytes. These gains complete relocation-target proofs
for existing machine code; no newly generated source body became exact.

| Function | Original bytes | Completed proof |
| --- | ---: | --- |
| `RxNextStateV17` | 730 | Decoded register-only SETcc in the existing pushed-frame proof |
| `V90ConstellationPower::getConstellationInfo` | 227 | One argument-load guard corridor and balanced push-only leaf frame |

The first extension admits only the exact unprefixed register-byte encoding,
preserving callee-save tracking; memory forms remain unsupported. The second
admits one decoded ESP-relative argument load that preserves flags and the
selector, and permits a frame without outgoing space only when there are no
calls. Existing guard-entry, table-bound, relocation and destination proofs
remain required. No anonymous target is accepted by masking its bytes.

[SETcc controls](recovery-setcc-proof.md) pass 128 positive cases and 14
refusals; [corridor/frame controls](recovery-corridor-proof.md) pass five
positives and 27 refusals. Each also verifies changed table targets remain
RELOC through the shared comparator. Existing ABI/frame/integration controls
pass. Independent review added eight SETcc refusal probes and nine corridor/
frame refusal probes, without finding a blocker. The parent reran both
dedicated control suites on the integrated tree.

## Source and negative-domain results

[Tone IIR](recovery-toneiir-interval.md) now stores the interval as one int,
preserves the full cadence producer and uses the original envelope shift.
The constructor still narrows at its short-argument helper boundary. A
constructor-created component lifecycle with interval 66,036 exposes the old
reconstruction: it reports an early verdict at sample index 499, whereas the
reference continues counting. The old-object negative control fails 134 of
66,172 checks; the retained source passes. This is a component configuration
test, not a public modem reachability claim. F46's historical short-storage
claim now carries an explicit correction.

Eight Tone IIR matrix cells and two cadence controls produced no exact gain.
The expanded-section seed was not adopted. The focused five-test selection
and the isolated full phase passed before integration. All configuration
offsets, extents and data bytes remain unchanged.

[CID terminal-cursor ownership](recovery-cid-terminal.md) closes without
source adoption. Four cells plus unchanged reproduction restore the witnessed
wrap dependencies but do not recover the complete body. The both-terminal
candidate passed six focused period binaries. Its source was then restored;
the replay tools and negative findings are retained.

## Integrated validation

`make phase partial-link J=4` with the published Gentoo image exits zero:
**388 passed, zero failed**, and all default structural checks pass. Static
mutation-locator validation reports 285 suites and 10,040 locators, with no
skipped, nonunique, vacuous or wrong-arm locators. No mutation campaign,
tolerance change or modern portability result is claimed.

All 300 canonical objects were hashed against the frozen opening manifest.
298 remain raw-identical; only `Cadence.c` and `toneiir.c` change. The complete
compiler configuration is unchanged. The canonical objects match the
independently tested candidates. The final census contains 1,093 EXACT,
662 SIZE, 65 BYTES, 29 REGALLOC and three UNRESOLVED names. The ratchet now
protects every final exact name; its three positive/negative controls pass.

Strict partial comparison still exits one: semantic and raw identity remain
DIFFERENT. It reports 69,775/943,398 equal positioned bytes, 421/2,931 exact
symbol records and 994/18,317 exact relocation records. Positioned equality
depends on layout and is not a source-recovery score.

Parent evidence is under `build/recovery-unblock/`: the frozen `baseline/`,
`integrated-gates.log`, control logs, `object-audit.json`,
`final-byteident.json`, `final-partial.json` and `final-checkpoint.json`.
The family reports locate the isolated candidate sources and detailed logs.
The final documentation and ratchet commit does not alter emitted source.

## Remaining work

The next proof targets are `V32LocLoopNextState` and
`V90Resampler::setBllState`: both require stronger fixed-frame/call/relocation
reasoning, and the latter also needs taken-edge guard dominance. Matching
ordered table entries alone does not settle them. `getSegmentPointer`'s
anonymous initializer identity remains a separate closed limitation.

For new source candidates, implement the planned per-call refusal ledger and
bounded CFG/private-carrier analysis. Do not repeat the closed Tone IIR or
CID source matrices without a distinct witness, or reopen the previous
packer spelling domains merely because their size gaps are small.
