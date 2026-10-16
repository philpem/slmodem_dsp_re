# Packer and resampler follow-up

Branch `investigate/source-recovery-packers`, based on `d9995470`.
This executes the approved MP encoder/decoder, CP encoder, paired DIL,
Class1 null-state and FixedRC follow-up queue. The starting exact set is
1,091/1,852 names and 122,715/720,125 original bytes. The preceding campaign's
five gains remain the baseline; they are not gains of this iteration.

**Final result:** zero new exact functions and zero exact regressions.
The integrated source at `c5d7b672` passes 388 period differential binaries
with zero failures and all default structural checks. Its fresh census
preserves all 1,091 exact names and 122,715 exact original bytes. The ratchet
is unchanged. These experiments recovered supported source graphs but did
not close any additional complete function.

## Evidence and acceptance

Every source domain declares its original witness, finite controls and
falsifier before compilation. Reconstruction commands retain the complete
Gentoo profile and append `DSPLIB_REPRODUCE_BUGS` through the shared helper.
Unchanged complete TUs reproduce raw before their variants are interpreted.
Full-body, data, binding, relocation and exact-membership collateral remain
visible, including diagnostic profiles and already nonexact bystanders.

The 300-object baseline is frozen at
`build/recovery-packers-next/baseline/`. Every input hash matches the previous
final checkpoint. The freeze reconstructs the old FixedRC object from its
independently raw-reproduced control after the live canonical directory was
updated by this iteration; that substitution was verified against the old
hash, rather than treating a mixed live directory as a pinned baseline.
The three agents also preserve their own independently checked baseline
objects and configurations.

## Closed investigations

| Family | Retained recovery | Limit |
| --- | --- | --- |
| [MP](recovery-mp-next-results.md) | Original predicate, cursor publication, complete sequence-length/print producers and padding-bound carrier | No complete exact gain; encoder SIZE19, decoder SIZE32 |
| [CP](recovery-cp-next-results.md) | Late CRC feedback, published bound, four literal count captures, scalar bounds and byte predicate | No complete exact gain; encoder SIZE37 |
| [DIL](recovery-dil-next-results.md) | V.92 payload pointers with separate separator owner, three info-loop producers and original parity CFG | No complete exact gain; V.92 SIZE3; V.90 untouched |
| [FixedRC](recovery-fixedrc-pair.md) and [generic FIR](recovery-fixedrc-polyphase-loop.md) | Two-short aggregate, terminal-history-pointer wrap owner, unsigned generic index and forward cursors | No complete exact gain; Resample SIZE264 |
| [Class1 null state](recovery-null-state-schedule.md) | No source adoption | Incoming pointer reload difference begins in global allocation/reload; removing late scheduling worsens it and loses existing exact matches |

MP's 25 complete-TU cells provide 400 shared-body verdicts. Its diagnostic
scheduling/profile alternatives are rejected. FixedRC's 24 cells provide
120 verdicts and six raw unchanged controls; both aggregate forms reproduce
the original partial-word mask/merge graph, with identical objects. Its
previous inline-helper-origin controls also fail in the new carrier context.
Class1's two diagnostic cells provide 12 verdicts and close that reserve
without a source or profile change. CP's 20 cells provide 300 verdicts;
DIL's 49 provide 49. Together these are **120 valid full-TU compilations
and 881 shared-body verdicts**, including repeated raw controls. The
denominator does not count native builds or failed/excluded diagnostics.

CP's whole-TU audit accounts for fourteen anonymous switch-table addends
shifting with the decoder's new start; relocation entries, targets and
function-relative destinations agree. Its other allocated nontext contents
are unchanged. It would be incorrect to describe those raw addends as equal.

These partials are accepted for independently measured original operands and
dataflow. A smaller SIZE gap alone is insufficient; SIZE counts bytes of
length difference, not differing bytes or instructions. FixedRC's retained
graph has a larger size gap than its opening source. No tolerance, fixture
assertion or mutation snapshot is changed to make these recoveries pass.

## Analysis gaps exposed

The retained compiler profile enables automatic inlining. Historical claims
that a strong GLOBAL symbol proves the original copied its body were based
on a different optimisation profile and cannot support that conclusion here.
MP's existing producers do auto-inline while preserving strong exports. CRC
helper substitutions can nevertheless introduce different widths or read
ages; those candidates are rejected on their measured graphs.

An indexed loop and a pointer loop can also publish through different owners.
In DIL, advancing the helper's only pointer fixes payload traversal but moves
the separator away from the original preserved base. Similarly, the original
FixedRC wrap start derives from the first loop's terminal pointer, while the
opening reconstruction discarded it. These are producer relationships that
a mnemonic count, size gap or declaration-order search does not identify.

Saved RTL needs operation and memory identity as well as UID searches. A
dump can retain an instruction in diagnostic text after the pass deletes it;
a first textual occurrence alone can misattribute the responsible pass.
Late scheduling is also not a general explanation for an argument-load
order difference already created during global reload.

## Final integration

The canonical configuration is unchanged. Exactly four of 300 objects
change: FixedRC.c, V90MP.cpp, V90CP.cpp and V92DILdesPCK.cpp. All other 296
objects remain raw-identical to the frozen baseline. Each selected canonical
object also raw-matches its independently compiled candidate, including MP
after the inlining-comment correction. No header, fixture assertion,
tolerance, mutation snapshot or production compiler profile changes.

`make phase partial-link J=4` with the published Gentoo image exits zero:
388/0 period binaries and all structural checks pass. Static mutation-locator
validation covers 285 suites and 10,040 locators, with zero skipped, repeated,
vacuous or wrong-arm anchors. This is not a mutation campaign or a mutation
score claim. Refcheck reports 14,466 references with zero unresolved/stale
entries. Modern portability was not run.

The final byte census is 1,091 EXACT, 662 SIZE, 65 BYTES, 29 REGALLOC and five
UNRESOLVED of 1,852 names. A fresh strict partial comparison exits one;
semantic and raw identity remain DIFFERENT. It reports 69,679/943,398 equal
positioned bytes, 421/2,931 exact symbol records and 994/18,317 exact
target-aware relocation records. Positioned equality fell from 70,072 at the
opening checkpoint because layout moved; it is not a source-recovery score.

Evidence is under `build/recovery-packers-next/`: `final-gates.log`,
`final-byteident.json`, `object-audit.json`, `final-partial.json` and
`final-checkpoint.json`. The checkpoint pins the source revision, all 300
object hashes, complete exact set, gate denominator and strict verdicts.

```sh
make phase partial-link J=4 TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
TC_OUT=build/tc_repro python3 tools/toolchain/byteident.py --ratchet
make partial-compare-exact
```

The final command remains an expected failure. Read each family report's
closed domains before a new experiment. Further work requires evidence for
the remaining Type/CRC spill interference, CP cursor store motion, DIL
base/extent lifetimes or FixedRC private-home/induction origin. Another
nearby declaration, loop spelling or per-file flag chosen by score is not
supported by this pass.
