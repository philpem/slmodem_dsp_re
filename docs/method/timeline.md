# Source-recovery timeline

This timeline records how the project progressed from an ELF survey to a
tested source replacement, then towards recovery of the original compiler
output. It covers the history through `1d4b891e`, merged on 16 October 2026.
For instructions on repeating the work, use the
[recovery workflow](recovering-source.md).

Dates are the cited commits' committer timestamps converted to UTC. They
identify when work was recorded or integrated, rather than when an
investigation began. Author dates can differ after a rebase, and parallel
branches can record results before their integration. This is a selection of
milestones, not a complete activity log.

## Milestones

| Date (2026, UTC) | Event | Progress and boundary | Evidence |
|---|---|---|---|
| 30 July | Survey the original object | Surviving `STT_FILE` records reveal the original compilation-unit structure. ELF inventory and attribution tools distinguish supported ownership from provisional guesses. | [Initial survey](https://github.com/philpem/slmodem_dsp_re/commit/a69c1f0457660e5a7e2bf0eaf6bdb397d5a861ef) |
| 30 July | Establish a differential harness | Rename reference symbols and separate stateful imports so both implementations receive equivalent inputs. G.711 provides the first exhaustive comparison. | [Harness and G.711](https://github.com/philpem/slmodem_dsp_re/commit/d7aef9c99f55685b88c2fe88fb3c1bb5aeb3ccf2) |
| 30 July | Connect the Bell 103 datapump | Reconstruct registration, configuration, DSP and host glue. The recorded test connects at 300 bit/s in each direction and recovers 906 bits; this is a concrete connection result, not a claim about every modem family. | [Datapump closure](https://github.com/philpem/slmodem_dsp_re/commit/d87f904668cbd02c206ab3046be19f4a7261d297) |
| 31 July | Add an independent interoperability peer | SpanDSP supplements comparison with the blob. The initial result validates the transmitter while leaving the receiver investigation open. | [SpanDSP interop](https://github.com/philpem/slmodem_dsp_re/commit/14f3e3ff322499f376f94640fe95c8f7008c11ee) |
| 2 August | Adopt the V.34 fast pass | Keep assembly reading and differential tests mandatory; defer coefficient derivations and longer investigations so callable components can land sooner. | [Sequencing decision](https://github.com/philpem/slmodem_dsp_re/commit/f1af60dc6161412fa1b97dd6ad361cbbd159e12e) |
| 4 August | Expose uniquely named local symbols to tests | Globalise local symbols before renaming them. This removes a supposed direct-testing limit, while duplicate local names still need separate treatment. | [Symbol-map correction](https://github.com/philpem/slmodem_dsp_re/commit/d915b73d1997ba5dd0ecf8caf232a58f3698f34f) |
| 6 August | Add a historical compiler and code-generation comparison | Docker hosts the initial GCC 3.x environment. Controlled options recover frame, outgoing-argument, scheduling and register-renaming behaviour. Early instruction-sequence scores are weaker than the later strict byte comparator. | [Initial toolchain](https://github.com/philpem/slmodem_dsp_re/commit/d2d3a23da5c7d09ded17b48c4690ad58c533cde5), [scheduling](https://github.com/philpem/slmodem_dsp_re/commit/e8135904d41d8e4a79457397e711d2e92b517615), [register renaming](https://github.com/philpem/slmodem_dsp_re/commit/f11c6719b8a10af1792332297eab9993452541af) |
| 7 August | Record mutation verdicts with dependency hashes | Snapshots make stale results detectable and expose uncaught or unusable mutations. A passing differential test and a mutation that detects a fault remain different observations. | [Snapshot and gate](https://github.com/philpem/slmodem_dsp_re/commit/a7f223292e9b78b2ec674ad15e182f5bbe86bb1f) |
| 7–8 August | Integrate and close the large V.34 handshake body | Complete dispatch and reachable arms, with explicit treatment of defensive guards. The closure also strengthens control-flow accounting and its negative controls. | [Integration](https://github.com/philpem/slmodem_dsp_re/commit/1d3ff0540c18d1cbe648103557bb8c2863381c34), [remaining-arm closure](https://github.com/philpem/slmodem_dsp_re/commit/047de88b190ff1627155bdb824363f6a25de723c) |
| 11 August | Make the period build the deciding differential tier | Compile the reconstruction and harness with the historical compiler family. This begins removing modern-compiler compensations from reconstructed source; the compiler's exact provenance is corrected later. | [Authority change](https://github.com/philpem/slmodem_dsp_re/commit/086122a959e6111fff828cb6c9a07004f5c01629) |
| 16 August | Correct the compiler version | Discover that the earlier container actually used Debian's GCC 3.4.4 prerelease. A stock GCC 3.4.2 control changes 18 of 183 complete objects and improves instruction-sequence matches; the Gentoo patch stack remains a separate question. | [Compiler A/B measurement](https://github.com/philpem/slmodem_dsp_re/commit/da26a2bc34c50a9f2197ce8095ca9febabb12970) |
| 23 August | Test a proposed decompiler-based equivalence approach | Ghidra 11.4.2 experiments expose a lost x87 operation in high P-Code, while raw P-Code retains it. The project keeps disassembly and executable comparisons as evidence and records the limits of lift-and-solve proposals. | [Measurement and recommendation](https://github.com/philpem/slmodem_dsp_re/commit/c48be9eb551af6dfdc3a6d8798b19539e886025c) |
| 30 August | Clear much of the remaining data-mode work | A merged V.22/V.32 wave reports 1,471 translated symbols and 293 passing period binaries. Span names prove unsafe as module boundaries; duplicate definitions from overlapping batches are caught during integration. | [Wave ledger](https://github.com/philpem/slmodem_dsp_re/commit/321a56e5c0b52b007f5b7bca29122f4f6f0afebf) |
| 3 September | Close the fax core service | The last Class 1 functions and top-level fax service entries land. Other inventory and measurement work continues after this service closure. | [Class 1 closure](https://github.com/philpem/slmodem_dsp_re/commit/e6772ef745fb81873a41a6961f38dd74989a230b), [service entries](https://github.com/philpem/slmodem_dsp_re/commit/31f577531324969fe522a2c443091da8c235c742) |
| 6 September | Close the project-owned function-name inventory | Coverage accounting credits an inlined-away function and separates bundled libm symbols. It reports 1,852 of 1,852 project function names represented. This does not establish that every internal path is reconstructed or exercised. | [Coverage correction](https://github.com/philpem/slmodem_dsp_re/commit/5a23550166586f9165224c05bba6c507e1b812ea) |
| 6–7 September | Strengthen byte-identity measurement | Guard against incomplete object sets, compare relocation meaning and protect exact-name membership. The subsequent checkpoint records a 775-name exact floor. | [Measurement repair](https://github.com/philpem/slmodem_dsp_re/commit/bf40be37ae2fa89b48be06f34608f47b3c3aa40c), [relocation identities](https://github.com/philpem/slmodem_dsp_re/commit/e0b0bc3112bd7e6a995f413e714b96f25e204b75), [floor](https://github.com/philpem/slmodem_dsp_re/commit/aeb4a5334e797a190b8fe7d4f59b9745a646a6d0) |
| 9–11 September | Adopt and publish the recovered Gentoo toolchain | Gentoo GCC 3.4.2-r2 becomes the default authority. The repository then consumes its published compiler image, separating toolchain reproduction from modem source reconstruction. | [Default change](https://github.com/philpem/slmodem_dsp_re/commit/e19ebb74eac8c753582e6e5fae221e49610a414e), [published-image integration](https://github.com/philpem/slmodem_dsp_re/commit/851d2c9698844c0c6c40ad0d56cd2685f95a63c1) |
| 20 September | Audit fixture validity at a lifecycle boundary | The P4D investigation distinguishes planted internal states from constructor-created histories. Synthetic probes remain useful, but their failures cannot establish public-call reachability. | [Fixture audit, PR #182](https://github.com/philpem/slmodem_dsp_re/pull/182) |
| 28–29 September | Reconcile data and local-symbol inventories | Recover 13 missing DATA globals from bytes and relocations, then close the local-symbol gap. These are inventory and ownership results, separate from function-body identity. | [DATA recovery](https://github.com/philpem/slmodem_dsp_re/commit/7585cb1d0489a1f634a67dcc51899a5df38686ce), [local-gap integration](https://github.com/philpem/slmodem_dsp_re/commit/bf246d56556546e5d54a7b1a4c1cf4fdf6e37368) |
| 2–4 October | Extend bounded relocation proofs and source recovery | Prove supported anonymous jump-table cases with adversarial controls. A later integrated batch adds 21 exact functions using witnessed source boundaries and complete-unit controls. | [Jump-table proof](https://github.com/philpem/slmodem_dsp_re/commit/6a6cda18bae02ed9299ed68c021a9a273f227e7a), [source batch, PR #261](https://github.com/philpem/slmodem_dsp_re/pull/261) |
| 7 October | Integrate the typed-structure cleanup | The owner-based raw-offset census and staged type recovery enter master. The original census counts 1,712 sites, while the issue's 215 entries count function/file pairs; those denominators must remain distinct. | [Integration](https://github.com/philpem/slmodem_dsp_re/commit/4783fc7f867c39ae362cc58413ea4a2f55b0b8ae), [census and wave record, PR #263](https://github.com/philpem/slmodem_dsp_re/pull/263) |
| 15 October | Retain witnessed partial source recoveries | PR #284 adds nine exact bodies and restores operand, owner, reload and publication boundaries in other non-exact bodies. Supported partial recovery is recorded even when a size gap grows. | [PR #284](https://github.com/philpem/slmodem_dsp_re/pull/284) |
| 16 October | Restore omitted FixedRC conversion and strengthen completion gates | PR #289 implements explicit modes 0/1, adds five exact source recoveries and completes two identity proofs for existing code. Semantic ELF equality and literal whole-file identity become separately enforced completion checks. | [PR #289](https://github.com/philpem/slmodem_dsp_re/pull/289) |
| 16 October | Consolidate the maintenance and method documentation | PR #288 integrates the latest source explanations, a module catalogue and reusable recovery guides. Historical reports move to Hindsight and pinned Git history; this is documentation progress rather than a new source-recovery gain. | [Documentation merge](https://github.com/philpem/slmodem_dsp_re/commit/1d4b891e071e7a75c49bcea6b82d6439cfd286da) |

## Reading the progress figures

These are historical checkpoints, not a freshly rebuilt series. Their units,
compiler provenance and comparison rules differ. In particular, a stored
ratchet can lag a fresh measurement, and extending a relocation proof can
recognise unchanged code as exact. Do not turn these figures into one smooth
percentage curve.

| Checkpoint | Recorded result | What it measures |
|---|---|---|
| 30 July | 161,028 passing checks | G.711 differential observations in the first harness; this is neither a function count nor a test-binary count. [Record](https://github.com/philpem/slmodem_dsp_re/commit/d7aef9c99f55685b88c2fe88fb3c1bb5aeb3ccf2) |
| 16 August | 330/924 instruction-sequence matches with stock GCC 3.4.2, versus 324/924 with the earlier compiler; 183/183 period binaries pass | The contemporary code-generation comparator and differential suite. The instruction score does not establish strict function-byte identity. [Record](https://github.com/philpem/slmodem_dsp_re/commit/da26a2bc34c50a9f2197ce8095ca9febabb12970) |
| 6 September | 1,852/1,852 project function names; 720,125/734,605 reference text bytes attributed, reported as 98.0% | Source inventory after accounting corrections and bundled-libm exclusions. It cannot establish coverage of every arm; the FixedRC work in October is a concrete example. [Record](https://github.com/philpem/slmodem_dsp_re/commit/5a23550166586f9165224c05bba6c507e1b812ea) |
| 7 September | 775/1,852 exact names | The recorded byte-identity ratchet floor at this revision. [Register](https://github.com/philpem/slmodem_dsp_re/blob/aeb4a5334e797a190b8fe7d4f59b9745a646a6d0/tools/toolchain/byteident_ratchet.json) |
| 4 October | 1,020 → 1,041/1,852 exact functions, no exact losses; 388 period binaries pass | PR #261's integrated Gentoo source-recovery measurement. [Record](https://github.com/philpem/slmodem_dsp_re/pull/261) |
| 15 October | 1,077 → 1,086/1,852 exact functions | PR #284's reported integration, alongside partial recoveries that remain non-exact. [Record](https://github.com/philpem/slmodem_dsp_re/pull/284) |
| 16 October | 1,086 → 1,093/1,852 exact names; 123,672/720,125 original bytes in exact functions; 388 period binaries pass | PR #289's fresh census. Five gains change source; two complete bounded identity proofs. Strict semantic and raw whole-object comparisons still fail. [Record](https://github.com/philpem/slmodem_dsp_re/pull/289) |

The closing checkpoint therefore records substantial function recovery while
leaving whole-object identity open. The
[current-work ledger](../remaining.md) and
[measurement guide](../maintainer/maintenance.md#measure-the-claim-you-are-making)
explain how to establish a new baseline.

## Identifiable agent clients and model credits

Tool provenance is incomplete. A commit trailer or session report establishes
an observed use or attribution, not continuous use throughout an interval.
The latest Claude marker and earliest OpenCode marker do not establish a
handover date. Configuration filenames and the repository's name are not
evidence of which client ran.

| Date observed (2026, UTC) | Tool or attribution | Evidence and limit |
|---|---|---|
| 30 July | Claude coding sessions | `fb99a4c4` records a `Claude-Session` trailer pointing to `claude.ai/code`; the same pattern appears in subsequent commits. This supports an attributed Claude coding session, without identifying a CLI/web front end or installed client version. [Commit](https://github.com/philpem/slmodem_dsp_re/commit/fb99a4c4cb360442a66c76f2e2432f25dad26fa1) |
| 19 September | OpenCode session | The mutation-runner change explicitly reports that an out-of-memory event killed an `opencode` session. This is earlier evidence of use than the later author labels. [Incident and repair](https://github.com/philpem/slmodem_dsp_re/commit/4f8259f58a9b5fc22307ef2791cce5f5c0d093e1) |
| 28 September | OpenCode commit attribution | The tooling commit records author `opencode <opencode@local>`; the following DATA recovery has the same attribution. An author label does not identify the underlying model. [Tooling](https://github.com/philpem/slmodem_dsp_re/commit/70bd87a09027184fdf6501ded08609686785ae6b), [DATA recovery](https://github.com/philpem/slmodem_dsp_re/commit/7585cb1d0489a1f634a67dcc51899a5df38686ce) |
| 16 October | Codex, this timeline's authoring session | The session's memory-runtime diagnostic reports `harness: codex`. This is contemporaneous session metadata recorded by the authoring agent, rather than a Git trailer. It establishes this documentation session's client, not Codex's first use on the project. |

Model credits are a separate source. The initial survey credits
`Claude Opus 5`, while a September commit credits `Claude Sonnet 5` and includes
a Claude session trailer. These are the labels stored in the commits; they
do not independently verify the model version or establish a complete model
history. See the [initial credit](https://github.com/philpem/slmodem_dsp_re/commit/a69c1f0457660e5a7e2bf0eaf6bdb397d5a861ef)
and [September credit](https://github.com/philpem/slmodem_dsp_re/commit/891740dc7c13cab8824b6b2f1b79d69ca71bd1f0).
No model is inferred for OpenCode or Codex from its client name.

The reconstruction tools are better documented than the agent history:
GNU binutils and Python support the original survey and harness; SpanDSP
provides an independent test peer; Docker hosts the period compiler; and
Ghidra is used for bounded experiments with its limitations recorded. The
milestones above link their introduction or use. The
[tool guide](tools.md) describes which parts can be reused on another project.

## Extending the record

For each addition, record the UTC date, an immutable commit or merged PR,
the observation and its scope. Preserve the distinction between a reported
result and a reproduced one. Include compiler/profile, numerator, denominator
and comparison semantics when giving a progress figure. A changed comparator
needs its own event so a measurement correction is not counted as recovered
source.

To inspect the first-parent integration history and a particular attribution,
run from the repository root:

```sh
git log --first-parent --format='%H %cI %s' 1d4b891e -- \
    README.md docs src include test tools Makefile
git show --no-patch --format=fuller fb99a4c4
```

`%cI` retains the recorded timezone offset; convert it to UTC before assigning
a calendar date. Inspect branch commits as well as merge commits when the
question concerns an experiment rather than integration. Silence in an
author field or trailer means the client is unidentified in that record.
