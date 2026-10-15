# Indirect callbacks and period-preprocessed loop results

Baseline80da7b0c, production1079/1852 EXACT unchanged. Gentoo3.4.2-r2 and
executed assembler2.15.92.0.2; preprocessing uses complete retained C/C++ flags,
dcr.c's per-file override, current headers and final DSPLIB_REPRODUCE_BUGS.
No reconstruction source/header/profile changes are adopted.
[Screen domain](callback-preprocessed-loop-domain.md).

## Scope and limits

All299 C/C++ TUs preprocessed and screened:1934 emitted body copies,781 eligible
nonexact copies. The parser recognizes537 project loops among1553 balanced for
headers;964 unsupported and52 outside the2..64-iteration domain. Only four
recognized loops originate in included headers, all in v34hstx1_arms.h.47 loops
have no emitted owner (including inlined/absent helper names); nine have ambiguous
emitted owners, retained explicitly.142 eligible body copies have matched loops.
No original inlining closure is inferred for the unowned header loops.

Recognized bounds support literals, parentheses and bounded +,-,*,shifts,&,|.
Macro constants expand under the period preprocessor. Enum identifiers, casts,
sizeof, variable bounds, descending/nonunit loops and other operators remain
unsupported. C++ leaf-name matching is a nomination aid, not clone ownership.
Seven detector controls pass, including original/retained putFrame18/6 indirect
sites, the known Precoder6/12-loop inventory and synthetic nested-header
provenance/nonunit/variable/equal-call controls. Initial missing DCR override and
an assertion expecting an old control among *new* nominations were preserved as
invalid, excluded, corrected and rerun.

Eleven nominations:

| Body | Witness/disposition |
|---|---|
| putFrame | Known18/6 callback positive; prior finite source domains remain closed |
| getFrame | New6/3 indirect sites; four fixed groups and captured callback tested |
| dftenergy | Hexadecimal0x12 loop, missed by decimal-only screen; loop remains; exit graph tested |
| fskdetect | Macro bounds; third-phase/transfer graph and delayed-tap read tested |
| FP_Pow | Macro7 expands; both original loops remain; retained seventh-coefficient guard explains extra branches; no repeat/source expansion |
| V90Demapper reset/resetNoSpectral | Macro6-loop nominations; existing receiving-byte domains not reopened from aggregate backedges |
| V90MP/V92CP evaluateCRC | Hex/macro16 loops; existing CRC shift/ownership controls remain closed |
| V90AutoDigitalImpDetector uniteLinMappInfoOfUnsuspectedPhases | Macro6/5 grouping loops nominated, not newly compiled/classified as source difference |
| faxvmi_hdlc_unframe | New lexical grouping nomination; screened only, fax lowest priority |

This does not classify every remaining body or establish a ceiling. A new
nomination is not a recovered source statement. The two uncompiled grouping
leads remain explicitly available for object-first review.

## getFrame: callback identity and fixed groups

[Eight-cell domain](getframe-callback-expansion-domain.md). Original1028B loads
callback at579dd and all six calls use the saved stack slot. Retained source
reads the member at each call and rolls four groups. Original four group
callbacks at57d24/57d3e/57d60/57d82 are separate regions, not a group loop.

| Fixed groups | Captured callback | Word locals | Bytes | Verdict |
|---|---|---|---:|---|
| no | no | no |597| SIZE431 |
| no | no | yes |597| SIZE431 |
| no | yes | no |633| SIZE395 |
| no | yes | yes |650| SIZE378 |
| yes | no | no |1036| SIZE8 |
| yes | no | yes |1046| SIZE18 |
| yes | yes | no |1041| SIZE13 |
| yes | yes | yes |1052| SIZE24 |

The audit confirms expanded six versus rolled three machine calls. Captured
forms mention the owner callback only once and use a saved stack slot at every
call; uncaptured forms mention the member at every call. These recover independent
operation/read-age witnesses without complete identity. No source adoption or
runtime claim about callbacks replacing themselves. All eleven bystanders,
metadata and allocated/named data unchanged;3/12 exact preserved.
**8 cells /96 common body verdicts**. This finite domain is closed.

## dftenergy: searched-index success/default exit

[Domain and count follow-up](dftenergy-loop-exit-domain.md). Original209B stores
k+1 on success then jumps to common bin advance; exhaustion stores20 then
falls through. Retained221B breaks and separately checks the exhausted index.
Explicit success jump yields215B/SIZE6; one post-search k+1 publication yields
207B/SIZE2. Neither is exact. Existing integer-energy-before-double-energy
publication already agrees with original; the store-order audit yields no fix.

The low-byte count witness motivates crossing all three exits with an unsigned
char working count and direct masked expressions. All nine count/exit cells
are target-raw-identical to their corresponding exit cell; &31 remains in every
form. Prior bare-mask removal/undefined-count tradeoffs are not reopened.
Both bystanders, metadata/data unchanged;2/3 exact preserved.
**3 opening cells /9 verdicts plus9 crossed cells /27 verdicts**.

## fskdetect: combined loop needs original read age

[Eight-cell domain and corrected read-age follow-up](fskdetect-phase-delay-domain.md).
Original692B combines the third phase's multiply with history transfer; retained
461B computes that phase then shifts history separately. Original computes the
delayed-tap pointer once at73bb9; original counters are sign-extended words.

| Combined transfer | Captured delayed tap | Short counters | Bytes | Verdict |
|---|---|---|---:|---|
| no | no | no |461| SIZE231 |
| no | no | yes |605| SIZE87 |
| no | yes | no |445| SIZE247 |
| no | yes | yes |605| SIZE87 |
| yes | no | no |429| SIZE263 |
| yes | no | yes |557| SIZE135 |
| yes | yes | no |429| SIZE263 |
| yes | yes | yes |557| SIZE135 |

The four opening combined-loop controls are rejected as source preimages:
original coefficient read73b56 precedes history store73b60, whereas these
controls emit the store first. Coefficients are writable shorts; preserve read
age before considering arithmetic equivalence. Five follow-up cells capture the
coefficient before transfer and recover that machine boundary, but remain429B/
SIZE263 or557B/SIZE135. The corrected controls are not raw-identical to the
rejected ones despite equal sizes. No production bug was introduced or fixed,
and no runtime alias/reachability result is claimed. Original, four negative
read-age controls and four corrected controls all fire in the bounded audit.
Only fskdetect changes; unchanged fskdemodulate and all metadata/data preserved.
**8 opening cells /16 verdicts plus5 corrected cells /10 verdicts**.

## Replay and validation

    python3 tools/callback_preprocess.py
    python3 tools/callback_loop_screen.py
    python3 tools/gcc3_getframe_callback_expansion.py --domain docs/getframe-callback-expansion-domain.md
    python3 tools/gcc3_dftenergy_loop_exit.py --domain docs/dftenergy-loop-exit-domain.md
    python3 tools/gcc3_dftenergy_count_exit.py --domain docs/dftenergy-loop-exit-domain.md
    python3 tools/gcc3_fskdetect_phase_delay.py --domain docs/fskdetect-phase-delay-domain.md
    python3 tools/gcc3_fskdetect_phase_read_age.py --domain docs/fskdetect-phase-delay-domain.md
    python3 tools/callback_control_audit.py

Preprocessed corpus/commands/hashes: build/callback-preprocessed/; nominations:
build/callback-loop-screen.json; full compiler dumps/commands/hashes/verdicts:
build/gcc3-* experiment directories named above. All five raw full-TU baselines
reproduce retained objects. Audit covers **33 cells /158 common body verdicts**,
complete symbol metadata, named data, allocated nontext and callback/read-age
positive/negative controls. Zero exact gains or losses. Tools compile, refcheck
and whitespace checks pass. No new runtime gate, fuzzing or mutation execution;
production remains the earlier388/0 period-validated tree.

Next evidence work: inspect the uncompiled ADID grouping loop and fax unframing
nominees, or trace getFrame's retained owner/capture transformations against its
remaining operand boundaries. Do not reopen these finite spelling products
solely because they got closer in size. Included header helpers without emitted
owners need a separate closure attribution, not automatic parent assignment.

Later partial-fidelity review: F11997 adopts the corrected fused FSK phase,
coefficient-read age, delayed tap and word counters after current full-TU replay
and fixed period validation. The negative exactness product above remains valid;
no neighbouring spelling/profile search is reopened. See
[current evidence](transmitter-budget-fidelity-results.md).
