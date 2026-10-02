# V8 coefficient-copy cursor recovery

At afedb41d, v8_copycoeff is41B against45B reference. Count and index are
already signed shorts. The blob advances independent source/destination
pointers by2, while retained source indexes both arrays through i.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955679871)
changes only the indexed assignment to the conventional *dst++ = *src++.
The existing short i/n loop, input/output widths and ascending order remain.
Nonpositive counts still execute no pointer operation. Positive n requires
valid n-word regions. Sequential ascending overlap behavior is preserved;
no memcpy/memmove replacement or wider alias promise is introduced.

Production raw-reproduces. The cursor candidate is45B EXACT, sole changed
body across13 functions and four named data objects; exact5→6/13, no losses.
All twelve sibling bodies/canonical relocations, symbol type/binding/
visibility/imports/exports, data values/offsets/targets and allocated nontext
controls agree. Two valid compiles/two raw emissions use actual saved flags
with DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed assembler
2.15.92.0.2. No flags or registers are forced.

    python3 tools/playbook_v8_copycoeff_cursor.py --domain DOMAIN_URL

Existing fixed t_v8util copy fixture makes66 paired calls (counts0..64 and
-3), comparing the complete64-word destination on each nonnegative count
and retaining negative no-op assertions:4162 checks. These are valid component
arrays, not asserted public modem histories. No new fixture, fuzzing or
mutation execution. The old comment incorrectly described counts above32767
as nonterminating although n itself is a signed short; it now states the
measured nonpositive no-op and ascending copy contract.

Production promotion reviews all300 objects; only V8global changes and
raw-matches the candidate. Artifacts: `build/playbook-v8-copycoeff-cursor`
(commands, RTL, full TU audit) and `build/playbook-v8-copycoeff-cursor-adoption`
(production promotion, census, full gate and complete same-order partial links).
F11651 records this recovery.

Whole-tree 922→923/1852 exact, 95,038→95,083 exact bytes, sole gain/no losses.
Fixed Gentoo make phase passes386 tests/0 failures and all structural checks;
static285 suites/10038 anchors, none detached/nonunique. The copy section
passes4162 checks. Complete same-order300-object partial links stay DIFFERENT
(strict exit1 before/after): positioned equal bytes68,596→68,594/943,398 (-2),
allocated914,782 unchanged. Exact section70/92, symbol394/2907 and relocation
1023/18317 records unchanged. Alignment absorbs the function's4-byte growth;
this source/function gain does not establish whole-object/profile identity.
