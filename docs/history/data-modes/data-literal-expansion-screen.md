# Data/service literal-expansion screen

Read-only reframe following the negative V32 publication/width domains.
Base remains 6b4509bd, 1069/1852 exact. Screened source scope: V32, V22,
V23, B103, V8, service, call and callprog C translation units with retained
objects. No V34, PR263 shared scope, fax, C++ or core/DSP source was edited.

`tools/data_literal_expansion_screen.py` counts direct call sites in original
and retained function disassemblies. A following PC-relative relocation
supplies the callee when present; resolved local callee labels are retained.
Calls to a function interior and indirect calls are excluded. Eligible bodies
are nonexact and at most 800 original bytes. Original >=2 and retained ==1
same-callee sites nominate a missing repeat graph for manual investigation;
a count alone never adopts source or establishes a missing source statement.

Denominator: **99 TUs, 389 emitted bodies, 146 eligible nonexact bodies**.
There are **zero nominations**. The detector fires on the independent C++
positive reference owned by the other agent: calcModulusParameters has five
__moddi3 and six __divdi3 original sites versus one and two retained sites.
The reference is read only; no C++ source is edited.

A supplementary fixed-small-loop source inventory found 23 loop occurrences
in 15 eligible functions. Closed DTMF, FDSP and Detector families were not
reopened. Detailed paired original/retained disassemblies for VTBv32_init,
reset_cid, biquad_filter, CID_process and v8_tone_detect retain corresponding
backward branches/iteration graphs; no missing straight-line literal
entry-store expansion is independently established. VTBv32_init has 22
immediate zero stores in either disassembly and both retain the metric loop;
reset_cid retains its clearing loops. Differences in scheduling, branch layout
or aggregate store counts are not sufficient to hypothesize literal expansion.
Other nominated source loops include variable-prefix B103 padding and
constellation decision scans, without a repeated-call nomination.

This screen does not cover original >800-byte bodies, indirect callbacks,
fully inlined helpers with no call sites, or every possible fixed-entry store
idiom. It does not show that remaining source is correct or unrecoverable.
Within the specified repeat discriminator there is no justified source family
to compile. Production source remains unchanged.

Run:

    python3 tools/data_literal_expansion_screen.py

The denominator, complete source inventory, empty nominations and positive
reference are preserved in build/data-literal-expansion-screen.json. Five
paired function disassemblies are in build/{function}-{blob,ours}.dis. No
compiler experiment, runtime, fuzzing or mutation run was started for this
screen. Python compilation and git diff --check pass.
