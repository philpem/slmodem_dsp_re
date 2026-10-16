# V32 receive phase-reversal graph: bounded negative result

Base: master 6b4509bd (1069/1852 exact). Scope is only V32rxhdx.c,
particularly RxHdxPhsReversal: blob 653 bytes, retained source 635. This
translation unit has 14 emitted bodies, 12 already exact. RxHdxTone is the
other nonexact body; it was not edited. Production source remains unchanged.

The paired original/retained disassemblies show independently observable
publication boundaries, rather than a reason to permute register spellings:

* The missing-tone counter is written to short_ac before the original memory
  comparison. Retained source computes a temporary miss and publishes later.
* Mode-zero RTT clamps the short candidate before field publication. Nonzero
  RTT publishes before its diagnostic, rereads owner/field after the external
  call, then clamps. Retained source shares the scale calculation and clamp.
* Original timer/short_a8 update shares a signed symbol_len extension;
  retained unsigned result cast leads to separate zero/sign extensions.
* Original nonzero RTT sign extension precedes its field store/debug guard;
  retained extension appears only on the diagnostic path.

The first source domain crosses persistent counter publication with arm-local
RTT calculation/publication/clamp (four complete TUs). The second crosses
that combined graph, signed compound short_a8 update, and captured short RTT
report (eight TUs). Both use retained Gentoo flags/config through shared
experiment helpers, bug define appended last, verbose compiler/selected
assembler provenance and saved preprocessed source/RTL. Raw baseline objects
repeat the retained object in both domains; the shared graph control repeats
raw between domains. No flags, declarations, headers or ABI are changed.

Neither domain gains a complete exact function. All twelve cells retain the
same twelve exact neighbors and change at most RxHdxPhsReversal. Target size
residuals are 18, 37, 18, 24 in the first domain; 18, 19, 18, 19, 24, 21, 24,
21 in the second. Size is recorded as a verdict, not an adoption criterion.
No production patch is proposed.

The balanced RTL trace audits four named fields across eight unambiguous
stages per cell (96 observations). The known persistent-publication control
fires: initial RTL has three short_ac stores instead of two. Three remain
through global allocation and flow2; two appear by sched2 and remain in final
mach. This bounds optimization of that explicit source graph after flow2 and
by sched2; it does not uniquely identify a pass or prove the blob's original
source. All twelve GCSE dump chunks contain two complete function streams
and are explicitly unparsed. The parser does not silently select a duplicate
instruction UID or pretend those stages were inspected.

The complete-object audit covers 12 TUs / 168 emitted-body verdicts, all named
symbol metadata/bindings/imports/exports, allocated data section shapes
(type/flags/alignment/size), NOBITS, and canonical non-text relocations. These
are unchanged except six controls reorder the same two diagnostic strings
in .rodata.str1.1. Their raw before/after bytes are preserved in the report;
section shape and the NUL-terminated string multiset remain identical. This
is an observed data change, not raw object identity. Text relocation targets
and all unchanged function bodies are compared through byteident helpers.

Reproduction:

    python3 tools/data_state_reversal_reproduce.py
    python3 tools/data_state_reversal_width_reproduce.py
    python3 tools/data_state_reversal_trace_audit.py

Artifacts reside under build/data-state-reversal,
build/data-state-reversal-width, build/data-state-graph-dis and
build/data-state-reversal-trace-audit.json. Compiler runs need the Gentoo
container. No runtime, fuzzing or mutation harness was run. Python compilation
and git diff --check pass for the tools.

Two negative domains close these particular source controls. A next useful
experiment needs a new independent discriminator in a different graph,
owner acquisition, helper factoring or loop boundary. A memory test or matching
short-extension fragment alone does not justify more neighboring spellings,
and this result does not establish a global byte-exactness ceiling.
