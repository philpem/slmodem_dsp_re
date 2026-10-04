# V34 updateAlpha quotient-width control

A fresh screen at900311af inspects six non-exact functions across two TUs,
v34scram.c (four functions) and V34TX.c (seven), for eleven complete bodies.
This is a bounded sample, not a classification of the remaining tree.
Only updateAlpha supplies an independent local width/stage hypothesis.

The blob’s IDIV at0x5d5fa is followed by CWTL and TEST AX,AX before the
negative fallback. Retained source already casts the quotient to short, but
stores it in an int local; production spills that value, reloads with MOVSWL
and tests EAX. The blob keeps the numerator and quotient in registers.
The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957139313)
changes only `int r` to `short r` in the complete V34TX TU. Parameters,
shift local, arithmetic/casts, diagnostics, stores and decay remain unchanged.

    python3 tools/playbook_v34_alpha_width.py --domain DOMAIN_URL
    python3 tools/playbook_v34_alpha_audit.py

Saved full flags, including DSPLIB_REPRODUCE_BUGS, use Gentoo GCC3.4.2-r2
and the executed assembler2.15.92.0.2. Raw production baseline reproduces.
Two valid compiles produce two distinct raw objects. Baseline204B and
candidate205B both miss the169B blob; exact1/7 stays unchanged, no losses.

Initial RTL changes the negative comparison from the full SI result to its
HI subregister. Combine selects cmphi_ccno instead of cmpsi_ccno. This
source-width explanation is observed before allocation. The allocated
arithmetic pseudo still uses SI, and the complete allocation/conflict/
preference/reload header is identical across cells. Numerator and quotient
spills both remain; the final object changes TEST EAX to TEST AX and grows
one byte. Do not report a spill recovery or allocator improvement.

The inlined copy changes adaptecho too: both795B versus755B reference,
alpha193 versus183 instruction rows. updateAlpha has61 versus57 rows in both
cells. Five other bodies/canonical relocations agree. Across all seven
functions and zero named data objects, symbol type/binding/visibility/import/
export records, allocated nontext extents, raw nontext and its relocations
agree. No source adoption or candidate runtime test is claimed. Production
remains925/1852 exact,95363 bytes, with its previously passing386/0 gate.

Artifacts under build/playbook-v34-alpha-width contain complete commands,
identity, both objects/dumps, normalized stage diffs and full-object audit.
Normalization removes only compiler heap addresses in declaration/string
and lexical-block metadata; register IDs, operands, constants and instructions
remain. An initial audit wrongly cut the diagnostic header at the first
NOTE after allocated instructions and failed its equality assertion. The
correct cut precedes the first allocated instruction; the complete normalized
global dump differs only at the predicted comparison and selected pattern.
The initial extraction failure is not allocation-divergence evidence.

Existing test/unit/t_v34rx.c supplies588 paired numeric calls (14 energies,
seven gains, three decay values, two decay settings) and42 paired debug calls
with85 checks. This is constructed component coverage with live alpha/tag
storage, not public modem reachability. Energy zero skips division. Known
small-negative divide traps and nonterminating normalization inputs remain
excluded; period signed-overflow/oversized-shift expressions are preserved,
not made into a portability claim. No new fixture execution, fuzzing or
mutation harness execution belongs to this domain.

The five other screened functions are declined: scrambleGPC/GPA root+4
addressing has no independently typed header subobject; descrambleGPC/GPA’s
shift mask is intentional compatibility and other differences are carriers;
txinit has matching calls/extents without a missing ownership boundary.
These observations do not exhaust their TU/profile regimes.

Close the two-cell quotient-width family. The word predicate is source-lowered,
but the local type does not explain the register-held quotient or complete
body. Do not expand shift widths, declaration order or numerator synonyms
from this score. Any later original-source claim needs independent evidence;
TEST AX alone does not prove a unique declaration. F11660 records the result.

The independent parameter-update boundary later changes this result:
F11691's unchanged-compiler GDB trace locates the local/global eviction,
and the blob's in-place ADD/SAR supports updating normalized energy before
the reciprocal. Crossed with this measured short quotient, the complete
169-byte function becomes exact. The quotient-width-only negative remains
valid; no neighboring width/declaration-order family was reopened.
[Tracing and four-cell cross](gcc3-reload-tracing.md).
