# Scheduler instrumentation: declared before compilation

Base240481e6; retained Gentoo full-TU profile. Two V8 controls only: unchanged
baseline and PR273 coefficient-first-expression candidate. Append scheduler
verbosity5 to diagnostic dump flags only. Require both raw objects identical
to the non-verbose controls; any difference invalidates interpretation as pure
instrumentation. No source/profile adoption, no V34/header/shared-gate writes,
#22 writes, fuzzing/mutation execution. PR263 observedef22230e is excluded.

Read scheduler dependencies and ready-queue rankings for the real first-tap
coefficient/sample load UIDs103/106 (after combine). Original coefficient-first
versus compiler sample-first is already established at33.sched2. Do not infer
original dependencies from our RTL or add volatile/alias/register constraints.
Match implementation discussion to GCC3 source version; installed Gentoo dump
is authority. Treat exposed ranking as a next-discriminator mechanism, not an
exact function gain. Full-TU bystander/binding/data checks and raw controls.

Result: both instrumented objects raw-repeat their non-verbose controls. All
8 emitted-body/common comparisons and metadata remain equal. In the installed
Gentoo scheduler table, coefficient103 and sample106 have priority35 and load
cost3 each; coefficient has4outgoing edges, sample5. Both enter the ready list
at cycle4; sample106 issues first. The last scheduled address subtract104 has
a one-cycle data dependency on106, while103 is independent, so both receive
the same dependency-class rank under the official GCC3 source. Same-block rank
hooks tie; after-reload sorting skips register-pressure weight.

Official releases/gcc-3.4.2 haifa-sched.c rank_for_schedule ranks priority, then
other hooks/dependency classes, then outgoing edge count, before original
instruction order. This explains the observed tie-break without selecting an
option or inventing a source dependency. The retrieved release source is not
a verified reconstruction of Gentoo's entire patch stack; installed diagnostics
and mandatory raw repeats establish the actual result. Source URLs/hashes are
recorded in build/scheduler-source/provenance.json.

Original coefficient-pointer register is reused for sample-address formation,
so original hard-register reuse can impose an anti-dependency absent from this
allocated candidate. That is a bounded explanation, not recovered original
RTL or permission to force scratch registers. No source/profile adoption from
this scheduler control. Audit: tools/batch_scheduler_trace_audit.py.

Replay with pinned240481e6 production objects/config:

```sh
python3 tools/batch_scheduler_source_fetch.py
python3 tools/batch_scheduler_trace_reproduce.py --domain docs/batch-scheduler-trace-domain.md --baseline-dir build/production-before --historical-headers
python3 tools/batch_scheduler_trace_audit.py
```

Source fetcher verifies3known SHA256 hashes before recording official release
provenance. Raw replay audit pins both independent non-verbose object hashes;
no sibling worktree layout is required.
