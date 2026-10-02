# MakeTxData count capture and postdecrement recovery

At d672e822, MakeTxData is 175 bytes against the blob’s 195. The blob
loads the signed-short count before dispatch and tests the old count in each
of the four single-symbol loops. The pair-symbol loop already agrees.
The [declared crossed domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5956280043)
changes these two independently observed boundaries without changing arguments,
constants, store order or the pair loop’s decrement by two.

| Count captured before switch | Single-symbol postdecrement | Bytes | Strict verdict |
| --- | --- | ---: | --- |
| No | No | 175 | SIZE20 |
| Yes | No | 175 | SIZE20 |
| No | Yes | 211 | SIZE16 |
| Yes | Yes | 195 | UNRESOLVED1 |

The combined conventional spelling is `short i = *count` before dispatch
and `while (i-- != 0)` in each single-symbol arm. Every nonrelocated body
byte agrees, including selector guard, dispatch, counter widths and flags.
The sole relocation is R_386_32 at body offset 24, pointing to an anonymous
five-entry .rodata jump table. Its base differs between the blob and this TU;
the strict comparator deliberately does not identify anonymous tables.

A separate bounded audit checks all five entry relocations, unique function
ownership, and complete body bytes. Both tables target MakeTxData offsets
65, 103, 131, 163 and 28. The audit fires on four cells: one full body/table
match and three known misses. This proves the named source domain, without
changing any automatic grade or census. Do not call it a strict exact gain.

    python3 tools/playbook_v22_txdata_controls.py --domain DOMAIN_URL
    python3 tools/playbook_v22_txdata_audit.py

The saved baseline flags include DSPLIB_REPRODUCE_BUGS. Gentoo GCC 3.4.2-r2
and the executed assembler 2.15.92.0.2 reproduce the retained raw object.
All four full-TU controls review ten functions and zero named data objects.
Only MakeTxData changes. Symbol types, binding, visibility, imports, exports,
allocated extents and nonrelocated nontext agree. Anonymous table entries
change within MakeTxData as its control layout changes; the combined entries
are independently checked against the blob, rather than merely masked.
Other nine bodies and canonical relocations agree. TU strict exact count
remains 3/10.

Production adoption checks all 300 objects; only v22prc changes and it
raw-matches the combined candidate. Strict whole-tree count remains
924/1852, with 95168 exact bytes and no losses. The fixed period/structural gate passes
386 tests with zero failures. Existing MakeTxData coverage makes 130 paired
calls/checks, with poisoned output slack and two negative-count cases.
Single-symbol -1 emits 65535 symbols; pair-symbol -2 emits 65534. Odd pair
counts are excluded because the original loop does not terminate. Invalid
selectors retain live count pointers: moving the load before dispatch is not
a claim that null count pointers are valid. The initialized half-duplex
fixture also uses this generator. No fuzzing or mutation execution is added.

The complete same-order 300-object partial links remain DIFFERENT (strict
exit 1). Positioned equal bytes change 68448→68589 of 943398; candidate
allocated bytes change 914766→914798. Exact section records stay 70/92,
symbol records 394/2907 and relocation records 1023/18317. These layout
measurements do not establish the original optimization profile.

Artifacts live in build/playbook-v22-txdata-controls and
build/playbook-v22-txdata-adoption. F11657 records the recovery. A later
general table resolver must establish guard/index extent, instruction-boundary
targets, unique ownership and entry identity, with negative controls. A named
exception or anonymous-addend masking would not justify an exact count.
