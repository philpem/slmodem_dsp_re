# Return extension mode across matched calls

## Scope and controlled detector

The screen uses all 300 reproduction objects pinned to `4169cb3b`, verified
against the campaign baseline manifest before and after the run. Three SRE
caller objects are restored from their raw-reproduced controls rather than
mixing later source winners into the opening baseline. No source family,
function-size or exactness filter is applied.

The bounded first-return-use primitive classifies signed/unsigned AL-to-int
and AX-to-int extensions, normalising CWTL with MOVSWL. It compares modes
after equal known-call sequences, including unambiguous local direct calls.
Equal sequence order only aligns ordinals; it does not prove equal arguments
or CFG paths. Unknown branches, tables, memory aliases, carrier overwrites,
intervening calls and budget exhaustion refuse. Other carrier uses do not
produce narrowing hypotheses.

Nine existing flow/refusal controls and six new mode controls pass. The new
positives distinguish equal-count signed versus unsigned extensions for both
word and byte returns. Identity, equivalent signed forms, different targets
and overwritten return carriers are negative controls. All three original
MRF/SRE receiver mismatches fire on the archived signed controls and disappear
on their independently byte-exact adopted objects.

```sh
python3 tools/recovery_return_mode_screen.py --self-test
python3 tools/recovery_return_mode_screen.py \
  --objects ARCHIVED_4169CB3B_TC_REPRO \
  --baseline-manifest BASELINE_MANIFEST_JSON \
  --h1-controls-dir build/recovery-sre-read-boundaries \
  --output build/recovery-return-mode/screen.json
```

The ledger's ELF ownership helper resolves local calls; no alternate target
resolver or byte scorer is introduced. Reports preserve every input hash,
reference count, sequence/extension/refusal denominator and nomination trace.

## Measured surface and nominations

The scan covers 1,934 emitted function definitions: 1,886 shared defining
copies covering all 1,852 reference names and 48 candidate-only bodies.
There are 1,746 equal known-call sequences containing 4,703 aligned calls;
140 shared bodies have unequal sequences. Of the aligned pairs, 147 establish
extension modes on both sides and 4,556 refuse. Ninety-five retained indirect
call sites have no established callee. Refusal-reason counts may overlap for
one pair and must not be added as disjoint totals.

Seven pairs disagree on extension mode. Three are the completed SRE recovery.
Four additional low-word result boundaries are independently visible:

| Caller / producer | Original first return use | Retained |
| --- | --- | --- |
| TxHdxStartV21 / ModDataV21 | MOVZWL at 0xa271d | MOVSWL |
| TxNoCarrierV27 / FPM_PPS_filter | MOVZWL at 0xa5fd5 | CWTL |
| TxHdxDataV27 / FIFO_read | MOVZWL at 0xa39e8 | MOVSWL |
| DemodDataB103 / FPM_FSD_demodulate | MOVZWL at 0x8fc80 | CWTL |

The first three lie in the old fax-excluding screen's rejected family. B.103
was eligible but the previous extra-extension-count test cannot distinguish
one signed extension from one unsigned extension. Large and V.34 bodies are
included in the denominator; no additional nomination is claimed there.

The old call-boundary ledger rejected pre-call proximity as a type witness.
Its TxHdxDataV27 row concerns selected callback budgets, and its B.103 row
concerns the separate MRF/AGC boundary. The present observations follow the
fresh return carrier at the named FIFO/FSD call and are distinct evidence.
No closed declaration/register/profile domain is reopened.

## Declared source domain

Test only a local unsigned-short result in each nominated caller, keeping
public prototypes, return declarations, control flow and all other fields
fixed. ModDataV21 and PPS already return unsigned-short counts. FIFO_read
computes unsigned-short take counts; FSD's producer counts written bits and
the original consumer explicitly zero-extends its low word. These observations
justify local narrowing controls without guessing an original API type or
asserting that high-bit counts are lifecycle-reachable.

There are eight complete-TU compiles: unchanged and unsigned-local for each
of four source files. Require raw baseline reproduction, the predicted first
use, full bystander/data/binding/target audits and deciding component period
tests before retention. Exactness is measured by byteident. A successful
local narrowing control does not imply a unique original local declaration.

```sh
python3 tools/recovery_return_mode_reproduce.py \
  --baseline-dir ARCHIVED_4169CB3B_TC_REPRO --historical-headers \
  --domain docs/recovery-return-mode-screen.md
```

After the declared domain, inspect each full residual once for a distinct
original operand, published owner or guarded read. Further spellings require
new evidence. If none appears, close the domain with its measured limits.

## Distinct follow-up witnesses

The eight local controls compile and raw-reproduce all four baselines. No
complete body becomes exact. The V27 no-carrier local change raw-merges its
baseline, so no inert source change is retained. V21's remaining inlined
next-state owner reads belong to the already closed publication domain; no
new source axis follows merely from its smaller gap.

V27 DATA's remaining original instructions compare unsigned word budget/count
operands and reload the source owner after FIFO_read. Its original rate
dispatch has separate branch arms. Original ScrambleDataV27 callers pass
unsigned word slots, while the wrapper independently narrows before its
signed core call. Test exactly four binary axes over the complete DATA TU:
consistent unsigned local count/result words and unsigned budget uses; fresh
post-FIFO source-owner use; switch-form rate dispatch; and int wrapper formal
with explicit signed-word core use. Add two complete wrapper-TU controls.
This is eighteen compiles. The wrapper must remain raw-identical, and every
other caller/bystander must be audited. This repeats the demonstrated slot/use
distinction, without changing the SDMv27 core API. F9952's synthetic bypass
probe and normal-driving limitations remain unchanged; no high-bit budget
reachability is asserted.

B.103's original FSD count is zero-extended into an int stack local; the short
storage trial instead adds an absent final MOVSWL. Its original loops use a
signed-word index and its child owner is reloaded across calls. Independently,
the second original AGC call passes literal 1 in its fourth slot; current
source passes the incoming sample count. Cross exactly these four witnesses:
unsigned FSD capture into the existing int local, short loop index, direct
child-owner expressions and literal AGC flag 1. This is sixteen full-TU cells.
The AGC argument correction restores the original call slot independently
of the score. Its fourth formal is unused in both the reconstructed body and
the original (no fourth-slot read), so no changed modem output is claimed.
Valid fixture coverage must exercise the call boundary before retention; no
impossible history may be used to justify source.

Run `recovery_v27_data_boundaries.py` or
`recovery_b103_return_boundaries.py` with the same pinned baseline,
historical-header and domain arguments. These bounded crosses do not license
neighbouring register, declaration-order or profile searches after a miss.

## Final distinct boundary crosses

The first B.103 cross still has three independently witnessed differences:
original epilogues read the full int result slot; loop comparisons promote the
signed-word index against the unsigned count without first casting count to
short; and the tone success arm falls through with failure out of line. Cross
these three binary axes on the four-witness candidate, plus an unchanged
baseline: nine full-TU compiles. The int-return header overlay must remain
consistent. The only source consumers either discard this result or narrow
it into a short local; original RxHdxDataB103 explicitly reads AX. Audit both
consumers and the complete TU. This representation is not a unique original
formal-type inference.

The V27 cross exposes a separate read-age witness: the original captures the
published budget immediately after FIFO_read, before writing status/flag
bytes, and carries that captured value into underrun subtraction/fill. Current
source reloads budget after the stores. Cross exactly early capture versus
existing reload with each wrapper representation on the witnessed unsigned,
fresh-owner, switch candidate: four cells plus unchanged baseline. Original
normal-arm subtraction still reloads budget after ModDataV27 and must remain
so. This is a five-compile final domain, not a declaration/register search.

The final B.103 cross produces one exact body only when all three final axes
are present. Before retaining the consistent header, compile the four other
source TUs that include it directly or through b103.h with unchanged versus
int-return header overlays (eight cells); require every complete object to be
raw-identical. Source consumer enumeration is scoped to src/include, and the
test reference declaration/result locals must also capture the full carrier.

## Measured result and stopping point

Only the B.103 winner is retained. `DemodDataB103` is exactly the original
568 bytes, including operands and relocation-normalised targets; its previous
body was 482 bytes. The complete TU moves from 7/17 to 8/17 exact functions,
with zero losses. Its other sixteen bodies, including both result consumers,
remain byte-identical to baseline. Allocated non-code bytes/metadata, object
symbol extents, symbol binding/visibility and relocation target sequences are
unchanged. Every other source TU including b103fp.h directly or via b103.h
also remains raw-identical under the header-only cross. The test reference
prototype and receive-result locals now capture the full return carrier.

The original full-width return-slot reads at 0x8fc37/0x8fc87, zero-extension
at 0x8fc80, signed-index/unsigned-count comparisons at 0x8face/0x8faf1 and
0x8fbb0/0x8fbb2, fresh owner reloads and tone-success fallthrough independently
support the representation. Exactness alone does not identify a unique
original formal type, source spelling or compiler invocation.

| Complete-TU domain | Compiles | Shared verdicts | New exact bodies |
| --- | ---: | ---: | ---: |
| Four caller-local result controls | 8 | 72 | 0 |
| B.103 result/index/owner/AGC cross | 16 | 272 | 0 |
| V27 DATA four-axis/wrapper cross | 18 | 168 | 0 |
| B.103 return/count/success-arm cross | 9 | 153 | 1 |
| V27 published-budget capture cross | 5 | 50 | 0 |
| Other B.103 header consumers | 8 | 14 | 0 |
| Total | 64 | 729 | 1 |

All thirteen unchanged whole-TU baselines reproduce raw, and all sixty-four
compiles succeed. Nontext/data/bindings/extent checks pass throughout; only
the nominated body may change. Three rejected V27 controls also reorder call
target sequences; the audit records those changes rather than calling them
collateral-free winners. Both wrapper-TU cells are raw-identical. The final
V27 early-capture controls have larger SIZE gaps (125/122 bytes); the finite
fax domains close without source/header changes. V21's smaller SIZE gap and
V27 no-carrier's inert spelling likewise produce no retained changes.

`recovery_return_mode_audit.py --output build/recovery-return-mode/audit.json`
regrades all sixty-four objects, verifies recorded source/object hashes and
all bystanders, and checks that B.103's original unsigned mode is restored by
the exact winner. The screen's nine flow/refusal controls, six mode controls
and six archived H1 positive/negative objects establish a non-vacuous detector.

The deciding Gentoo period command passes 6/6 tests:

```sh
make period T='t_b103fp t_b103hdx t_b103direct t_b103create t_b103dp t_b103link' J=8
```

`t_b103fp` compares 150 independently constructed receive frames, including
acquisition through states 0/5/10/15/16 and positive demodulated bit output.
The incoming fragment is at most 160 samples and the bounded FSD producer
returns a handful of bits. These are component-boundary fixtures with real
objects/tone producers, not a high-count lifecycle claim. Existing `t_b103hdx`
state probes retain their original fixture limits. Anchorcheck reports 285
suites, 10,040 anchors, zero skipped/bad anchors; refcheck checks 14,457
references with zero unresolved/stale entries. The parent campaign owns the
final integrated phase, census and partial-link result; this focused result
does not claim final-object semantic or raw identity.

Artifacts are the named `build/recovery-*/results.json` reports, per-cell
sources/objects/commands, `build/recovery-return-mode/{screen,audit}.json`, and
`/tmp/recovery-baseline-audit/*return*`, `*v27*`, `*b103*` logs. One V27
generator run stopped before compiling because a source-only uniqueness
assertion did not account for a header-only axis; its log is preserved as
`v27-published-budget-invalid-generator.log`. One period invocation included
an unknown test name; selection correctly refused all tests, with its log
preserved as `b103-return-period-invalid-selection.log`. Both invalid runs
are excluded from the denominators above. No further source spelling,
register, profile or reachability domain is opened by these misses.
