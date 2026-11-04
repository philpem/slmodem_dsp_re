# Read-only function difference vectors

This report complements the strict fidelity grader with independent diagnostic
axes. It does not award a grade, combine axes into a score or establish
functional equivalence. Keep the period differential result and its input and
memory context beside the report. Use the existing whole-object
`partialcmp.py` report for section/data layout and final raw-file identity.

## Invocation

```sh
python3 tools/toolchain/diffvector.py \
  --reference ref/slmodemd/dsplibs.o \
  --candidate build/tc_repro/src_dsp_FP_math.c.o \
  --symbol FP_Pow --json build/fp-pow-vector.json
```

Repeat `--candidate` to report more defining copies. The report covers only
those explicit inputs; it does not choose a best copy or claim a tree-wide
symbol result. The normal census remains the authority for all defining
copies and link-selected proofs. Input hashes identify the compared artifacts;
ensure their compiler/profile and source provenance are current before using
an observation to select a reconstruction change.

A successful process exit means the diagnostic report was produced; it does
not mean the functions match. An unavailable owned vector returns nonzero.

Version one supports straight-line code and validated direct branches. Inspect
the reported mapping policy, ambiguity and unknown counts before interpreting
matched records. A chosen heuristic pairing is a diagnostic correspondence,
not a proof of unique source or value flow. Unsupported indirect destinations
and unproved stack-slot correspondence remain unknown.

Section placement, declared symbol extent, decoded instructions and recognised
padding are separate observations. A branch's byte encoding can change while
its mapped successor stays the same. Preserve width, signedness, address
components, relocation type/target/addend, partial-register lanes and x87
operands. Literal register-token agreement is distinct from the existing
renaming certificate. Stack observations are literal operand text, including
relocated immediates; they are not inferred stack-slot assignments. CFG blocks
split at calls and describe normal completion, excluding exception/interrupt
paths.

Run the maintained positive and negative controls with:

```sh
make diffvector-selftest
```

The target is part of `make phase` and the Gentoo CI apparatus controls. The
[design assessment](../metric-vector-assessment.md) records the intended axes,
known ambiguity and boundaries. Implementation results and a bounded manual
REGALLOC trial are recorded with the accompanying findings.

## Measured examples

These examples use the faithful Gentoo reconstruction objects at `d61e5e8c8`.
All paired figures below are reference/candidate. Ordinary instruction counts
exclude recognised padding; diagnostic mapping itself retains padding.

| Observation | FP_Pow | ApplyBulkDelay | GetFP_Value |
|---|---:|---:|---:|
| Declared bytes | 143/143 | 467/467 | 82/82 |
| Ordinary instructions | 55/55 | 103/103 | 38/38 |
| Blocks under the stated convention | 8/8 | 34/34 | 6/6 |
| Mapped edge records agreeing | 13/13 | 52/52 | 8/9 |
| Explicit GP-bearing rows agreeing | 49/49 | 74/78 | 23 matched, 9 mismatched, 2/1 unpaired |
| Existing copy grade | EXACT | REGALLOC | NONMATCH |

Every mapping remains heuristic with uniqueness unproved. GetFP_Value's one
mapped-edge mismatch is an observation under that mapping, not a proof of
different CFG topology. Its equal byte, instruction and block counts do not
explain away the differences. Typed decoding is partial: unsupported operand
effects remain unknown even when literal mnemonics and registers are available.

FP_Pow's raw encoding agrees on 54/56 decoded rows; its two relocation fields
encode different object placements. Both canonical relocation destinations
agree, and the existing grade remains EXACT. ApplyBulkDelay's four differing
GP-bearing rows are visible separately from its 52 agreeing edge records and
17 agreeing relocation destinations. This is why raw encodings, destination
identity and literal register observations need separate axes.

A corpus check selected the 39 REGALLOC symbols from the preceding faithful
census and supplied all **41 defining copies**. All 41 produce vectors; their
local grades are 39 REGALLOC and two EXACT copies, consistent with the census's
conservative symbol results. No known mapped edge mismatches occur. Thirteen
copies retain 33 unknown paired edge records, so this does not establish complete
CFG equivalence. This is a selected diagnostic corpus, not a new global census.

## Controls and known limits

The maintained suite has 19 methods and is run through unittest discovery. An
independent corpus contains 17 ELF pairs and 539 count/feature assertions.
Sixteen pairs produce vectors; the anonymous-prefix disassembly disagreement
is explicitly unavailable. A valid named preceding function supplies the
placement positive: raw function encoding agrees while placement differs.
Controls distinguish short/near branch encoding from a changed successor,
relocation addends, field offsets, scales, partial-register lanes, x87 operands
and widths, insertions, repeated anchors, and unsupported indirect targets.

Independent review caught an indirect PC32 call incorrectly inferred as a
direct callee and an address field masked by an unrelated immediate relocation.
Both are fixed and have maintained controls. API calls restore the grader's
link-resolution context and clear the affected caches. JSON output refuses
reference/candidate aliases, including hard links. Known-empty feature lists
are non-applicable, rather than fictitious matching operands.

Two remaining boundaries are deliberate. The existing symbol disassembler can
include anonymous prefix padding before a shifted function; if its boundaries
disagree with the owned decoder, the vector is unavailable. The existing raw
verdict is retained if independently available. No parser or grading rule is
relaxed to fill this diagnostic gap. Linked proof artifacts are not borrowed;
external proof context remains absent and unresolved evidence remains visible.

A separate existing grader defect is exposed by the vector (F12141):

```asm
/* reference */  mov %al,(%edx); ret
/* candidate */  mov %ah,(%edx); ret
```

The existing renaming checker labels this pair REGALLOC. With the explicit
machine input EAX=0x1234 and EDX pointing to an allocated byte, the stores
produce 0x34 and 0x12 respectively. The vector flags the literal and typed lane
difference without changing the reported existing grade. This is a controlled
machine-contract counterexample, not a claim about reachable modem states.
A targeted screen of all 39 current REGALLOC symbols finds no explicit paired
high/low-byte mismatch; its known negative and low/low, high/high positives
fire as intended. That screen is not a general ABI or dataflow audit.

Harden the renaming check with lane-sensitive controls before expanding
automated source search. Then use the vectors to triage structural residuals,
leaving ambiguous or unsupported mappings in a review queue. No source function
is promoted by this reporting change. Local evidence is retained under
`scratch/diagnostic-vector-trial/` in the investigation worktree.
