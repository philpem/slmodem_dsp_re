# V8 DFT-update sample cursor control

At379d400b, v8_dftupdate is150B against164B reference. The blob retains an
advancing sample pointer, loads directly through it inside the bin loop and
advances by2 once per sample. Retained source uses samples[j]. Bin traversal
already advances16 in both emissions, so another bin-cursor test is unjustified.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955611804)
changes only outer traversal to samples++ and inner load to *samples. Both
short counters/arguments, all phase/table/arithmetic statements and sample
load's inner-loop lifetime remain. No hoisting across bin writes. Valid input
is arrays of nsamples samples and nbins bins; the candidate does not establish
a new null-sample/zero-bin contract.

Production raw-reproduces. Cursor restores direct sample load, outer ADD2 and
spilled signed-short outer counter but grows to172B/SIZE8, with53 alpha rows
against51 reference. Complete body misses; exact2/3 unchanged, no gains or
losses. Only update changes across3 functions/1 named data object; all other
bodies/relocations, symbol type/binding/visibility/imports/exports, table
values/offsets and allocated nontext controls agree. Two valid compiles/two
raw emissions use complete saved flags with DSPLIB_REPRODUCE_BUGS, Gentoo
GCC3.4.2-r2 and executed assembler2.15.92.0.2.

    python3 tools/playbook_v8_dftupdate_cursor.py --domain DOMAIN_URL

Artifacts: `build/playbook-v8-dftupdate-cursor`, including full-TU audit.
No source adoption, new fixture execution or new differential claim for this
rejected cell. Existing fixed update fixture has1081 checks; that is coverage
context, not a candidate test. Close this finite sample-traversal family
without counter/declaration/register or table-index arithmetic synonyms.
F11650 records the negative control.
