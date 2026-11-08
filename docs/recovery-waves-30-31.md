# Waves 30–31: callback history and an alias-analysis discriminator

Base: `69601d93ec888e0cb5af9363a13e836de26a2a60` (PR #411). Issues
[#410](https://github.com/philpem/slmodem_dsp_re/issues/410) and
[#409](https://github.com/philpem/slmodem_dsp_re/issues/409) carry the fixture
contract and continuation. Reconstruction source and retained compiler flags
are unchanged.

## Wave 30: constructor-to-training at the decision-component boundary

`t_v27fax::run_constructor_history` starts each side with its own `V27RX_create`,
using `V27RX_CFG` with the supported 2400/4800 bit rates and short/long training
selectors. It invokes the callback currently installed by that side. The caller
writes only the current FSE I/Q input slot and supplies current angle/magnitude
arguments. Decoder history, counters, tap count and live callbacks are not planted
or replaced. Pointer normalisation affects comparison snapshots only; table,
owner and callback identities are checked separately.

This is a decision-component caller-input history. It does not execute
`FPM_FSE_receive`, the receiver AGC or a modem waveform lifecycle. Quiet I/Q
followed by an amplitude step drives epoch detection into training; subsequent
angle anchors drive the real training callback into the running decision.

The original `FPM_FSE_receive` doubles its reduced angle at `0xa80e9` before
indirect callback dispatch at `0xa81b2`. The retained history therefore supplies
even angles in `[0,32766]`, including error neighbours 8190/8192/8194. Odd values
appear only in the separately labelled detector controls.

All four constructor configurations enter training after epoch call 49 and
complete after 1000 long or 50 short training calls, followed by eight running
decisions. Total: **2328 paired callback calls**, **31756 history assertions**
and **42 known-answer detector assertions**. Per rate:

| Observation | Long training | Short training |
|---|---:|---:|
| Error magnitude below / at / above 8192 | 438 / 62 / 500 | 21 / 4 / 25 |
| Exact error 8190 / 8192 / 8194 | 63 / 62 / 63 | 4 / 4 / 4 |
| High-fold branch taken | 62 | 2 |
| Raw difference +16384 / -16384 | 157 / 187 | 8 / 9 |
| Low-fold branch taken | 0 | 0 |

These error magnitudes are phase differences, not the callback's output
magnitude constant `0x3299`. The low-fold branch is unobserved in this history:
the constructor-derived previous training decisions and non-negative supplied
angles do not produce a raw difference below -16384. Existing seeded probes
remain the evidence for wider input domains.

The observation helper duplicates the native arithmetic, so its classifications
have independent fixed-answer controls. Temporarily changing its `<8192` bin
test to `<=8192` fails 1/42 controls. Changing both strict fold tests to inclusive
tests fails 4/42. Both controls make the period binary fail; neither perturbation
is retained. Independent read-only review verified the constructor provenance,
callback chain, snapshot-only normalisation, native instructions and final
measurement counts. Earlier exploratory logs with smaller assertion counts are
superseded by the final fixture and gate logs.

The existing test binary is extended; the expected full-period denominator stays
394. This supplies #410's requested component-boundary witness, not a claim of
waveform reachability or coverage of every training boundary.

The final `make phase J=8` run passes **394 period differential binaries, zero
failures**, with all structural checks passing. The fixture adds assertions to an
existing binary; it does not increase the binary denominator.

## Wave 31: separate alias analysis from another INFO0 spelling family

The two INFO0 syntax families in the preceding report remain closed. A new
four-cell diagnostic crosses their unchanged captured source and saved
live/direct-explicit source with retained aliasing and `-fno-strict-aliasing`.
The source/flag axes are separate, and the prediction was declared before
compilation: withdrawing type-based alias analysis should restore the final
session `+0x6120` reload only in the live-source arm if that analysis explains
its elimination.

All four complete TUs compile: **4 objects / 28 function verdicts**. The retained
unchanged control reproduces all seven production bodies. INFO0 sizes are:

| Source | Retained flags | No strict aliasing | Reference |
|---|---:|---:|---:|
| Captured control | 665 | 661 | 758 |
| Live/direct-explicit | 758, 118 byte differences | 774 | 758 |

The decisive result is the load, not a size score. Under retained flags, the
live-source session read appears in `07.addressof`, instruction 357, then global
CSE replaces it with register reuse in `08.gcse` (instruction 509). With aliasing
withdrawn, instruction 357 remains a memory read. Assembly confirms the extra
`+0x6120` load at object offset `0x4fa`, after the `is_short` store at `0x4e6`.
The reference independently reads that field after its store, at `0x820b`.

This measures an alias-analysis-sensitive elimination in our compiler. It does
not recover the unavailable original RTL, select a global original flag profile,
establish which original source alias representation supplied the load, or make
the 774-byte candidate exact. Withdrawing aliasing changes four of seven bodies
in both source arms (`V34GiveINFO0dBits`, `V34GiveINFO1aBits`, `V34SetINFO0aBits`,
`VPcmV34InterpretMohMessageBits`); the complete ledger preserves those effects.
No experimental source or flag is adopted. The next discriminator is independently
witnessed alias/owner representation or a broader original-profile control,
rather than another nearby spelling or artificial volatile access.

Experiments use Gentoo GCC 3.4.2-r2 and assembler 2.15.92.0.2 (20040927), with
flags read from the production `.build-config` and `DSPLIB_REPRODUCE_BUGS`
appended last. An initial attempt requesting unsupported `-fdump-tree-all`
failed before producing a valid object; that log is retained and excluded. The
valid rerun uses RTL dumps (`-da`) and saved assembly.

## Evidence

Artifacts live under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/`:
`w30-v27/notes.md`, focused and negative-control period logs, native disassembly,
independent review, `w31-alias-valid.log`, `w31-alias/results.json`, RTL dumps,
and `w31-info-{retained,noalias}.s`. The session handover records final gate and
delivery details.
