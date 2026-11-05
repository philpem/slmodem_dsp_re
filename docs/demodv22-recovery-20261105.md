# DemodDataV22 recovery — 2026-11-05

`DemodDataV22` reaches EXACT at its native 510 bytes under both tested HDX
representations. The selected coherent change also permits the HDX alternative
to restore the two neighboring byte scores without its former Demod regression.
All thirteen other V22int bodies remain unchanged. The complete SRE and FSE
objects remain byte-identical to their landed controls.

## Three interacting boundaries

The native caller zero-extends the MRF and SRE output counts before forwarding
them. Our signed-short SRE/FSE declarations inserted two non-native sign
extensions. Unsigned-short parameters recover the caller's transport while
existing signed-short locals preserve the callees' low-word interpretation.
A signed local does not establish a signed formal parameter. The native caller
and callee must be checked together; the original formal declaration is not
uniquely determined by these instructions.

The native FSE entry at 0x8c5ae sign-extends the argument's low word, saves that
producer, and publishes its low word into n_in at 0x8c625/0x8c631. Publishing
from the existing signed `left` local preserves that producer. Changing only
the parameter introduced a different unsigned load and later sign extension.
Using `state->n_in = left` restores the entire four-function FSE object.
The three-function SRE object is already raw-identical after its parameter
change. Negative low-word values retain their old signed local behavior.

Inside the protocol-zero disconnect arm, native code retains a short RMS
result and promotes it before its short comparison. An explicit `short level`
preserves this lifetime and the native CWTL; an int local does not. This is a
bounded source-family result, not proof of the author's exact variable name.

DSP-owner captures follow native call/publication groups. In particular,
protocol!=0 jumps from 0x8e3d9 directly to 0x8e413, bypassing the DSP reload
at 0x8e410. On the RMS path both exits reacquire DSP after the call. An
unconditional reload changes the native graph; explicit conditional recapture
recovers it. The post-SRE owner survives the adapting publications through
the FSE call, and a fresh owner supplies the final count after FSE returns.

## Crossed controls

Fifteen full-V22int cells and six callee controls completed: 21 successful
compilations. The unchanged baseline was reproduced raw. All commands,
source/header/object hashes and stage evidence are retained in the ledger.

| Source experiment under the landed header | Result |
| --- | --- |
| Baseline | BYTES231 |
| Count interfaces alone | BYTES29 |
| RMS short capture alone, old count interfaces | BYTES202 |
| Counts plus short RMS capture | EXACT510 |
| Counts plus int RMS capture | BYTES29 |

The initial owner-only cross produced no promotion: explicit groups alone
were raw-inert under the landed header; header alone gave SIZE16; both gave
native length with BYTES253. Counts plus owner groups still did not recover
the alternative. Once RMS narrowing was settled, unconditional recapture
left BYTES25; the native conditional recapture reached EXACT under both
headers. Thus the initial regression was a useful lead into adjacent interface
and source-lifetime corrections, rather than a reason to reject the header
in isolation.

The final two V22int objects are literally identical:
`ea74fee2f212d9cb060726806dc043894b4109ab855ca1930d1e27b85515a177`.
All thirteen neighboring bodies, emitted order, exports and allocated data
remain unchanged. Independent review rescanned every native relocation
section: exactly two production references target the two count interfaces,
both from DemodDataV22 (0x8e46f SRE and 0x8e4d2 FSE), with no address-taking
reference. This does not uniquely recover the original parameter types.

## Header decision and limits

The chosen owner view is an explicit reconstruction of the observed alias
contract. It is not a claim that the original author declared a pointer union
or a generic pointer. Plain void* did not reproduce the union's effect.
The alternative restores ordinary HDX state words and their two known spill
schedules while explicit native captures preserve control, status and demod
owner epochs. The count/RMS recovery independently succeeds under the previous
header too; EXACT Demod alone cannot justify the header decision.

The full 300-object comparison changes only four objects: V22 create,
V22int, v22mod and v22prc. Control/status, SRE and FSE objects stay raw-identical.
Besides Demod's new EXACT result and the two restored BYTES scores, create,
Detect_v22 and four state handlers retain non-exact scheduling/register
collateral. There are no EXACT/REGALLOC membership losses. Exports, masked
allocated payload/layout and strict code targets remain unchanged.

## Differential scope and transfer

`t_v22rate` supplies 480 Demod calls per side across eight constructor graphs:
four unfrozen component histories (240 blocks) and four frozen, planted
exploratory histories (240 blocks). The latter are synthetic, not public modem
reachability. `t_v22recv` includes the negative-count boundary; unsigned
transport preserves its low word and signed local interpretation. The full
period gate supplies separate broader fixture coverage.

The next recovery turn should apply the lesson to Detect_v22 and, if native
boundaries justify it, V22FP_create: trace the consumers changed by a shared
declaration, not just the selected function. Require original owner/read-age
or interface evidence before transfer. Preserve negative controls and test
baseline/A/B/A+B before rejecting isolated regressions. Reopen this family
for new native input/type/owner evidence, not another register spelling palette.

Evidence: `/mnt/zfs/dev/slmodem/scratch/demodv22-combined-20261105/summary.json`,
the independent `v22-deep-20261105/demod-review/review.json`, and the parent's
`build/demodv22-combined/` comparison, census, input hashes and phase log.
[Issue #590](https://github.com/philpem/slmodem_dsp_re/issues/590) tracks the
remaining alias-declaration and consumer questions.

## Integration result

`nohup make phase J=8` passed: 399 period tests, zero failures, plus all
structural checks. Final census: 1,134 EXACT / 28 REGALLOC / 73 BYTES /
617 SIZE over 1,852. DemodDataV22 is the only new exact member against
4246de2a4; no EXACT or REGALLOC member is lost. No mutation snapshot refresh
or mutation campaign was run. The comparison verifies the actual built source,
including the clarified comments and removed redundant count casts.
