# Documentation

Start with the [Maintainer Manual](maintainer/index.md). It explains modem
operation, ownership, lifecycle and the algorithms used by each module.

| Material | Home | Use |
|---|---|---|
| Module responsibilities and algorithms | [Module catalogue](maintainer/modules.md), [signal paths](maintainer/signal-processing.md), [V.34 training](maintainer/training-algorithms.md) | Trace samples, bits and control through the implementation |
| Configuration and reference | [Configuration](maintainer/configuration.md), [parameters](maintainer/parameters.md), [terminology](maintainer/glossary.md), [call-progress tables](maintainer/call-progress.md) | Interpret fields, units and state transitions |
| Filter design and clock assumptions | [Coefficients](maintainer/coefficients.md), [sample rates](maintainer/sample-rates.md), [resampler banks](maintainer/resampler-banks.md) | Understand the retained tables and the separate retarget work |
| Maintenance procedure | [Maintenance](maintainer/maintenance.md), [reconstruction method](method/README.md) | Establish evidence boundaries and run the deciding gates |
| Repeating the work on another project | [Recovery workflow](method/recovering-source.md), [object-reading techniques](method/reading-object-code.md), [GCC build recovery](method/recovering-build.md) | Recover interfaces, source structure and behaviour from a compiled object |
| Current objectives | [Phase ledger and remaining work](remaining.md) | Separate completed source coverage from validation and final-object identity |
| Primary evidence | [Findings](findings.md), [deviations](deviations.md), [attribution](attribution.md), [input modules](modules.md) | Check observations and their original scope |
| Generated measurements and registers | [Coverage](coverage.md), attribution JSON, issue ledgers and the [portability review](tolerance-split-register-review.md) | Reproduce measurements; counts apply to their stated baseline |
| Deployment tools | [Blob fixes](blobfix.md), [hybrid blob builds](forkblob.md), [interop](interop.md) | Use these specific workflows |
| Salvaged reference material | [V.92 CP salvage](salvage/v92cp/README.md) | Understand recovered reference material and its limits |

## Where new material belongs

Put explanations of current modules, algorithms, contracts and maintenance
tasks in the manual. Put general reconstruction technique in `method/`.
Keep machine-consumed registers and generated reports at the paths their
tools use. Findings and deviations retain stable identifiers and their
correction history.

Save dated experiments, completed plans, durable decisions and negative
results to Hindsight with their source path, baseline, controls and measured
denominators. Incorporate useful current explanations into the manual, then
remove redundant standalone notes. Do not keep a second repository archive
of the records saved to memory.

## Historical evidence

The 16 October 2026 consolidation saved 715 investigation records to Hindsight
in 53 bundles titled `Documentation history migration (4169cb3b), bundle N of 53`.
The seven original technical references were also saved separately. Query
Hindsight with the original filename, symbol or investigation topic. These
records describe their original revisions; verify their claims against current
source before resuming work.

Git retains the exact original files at `4169cb3b`. For example:

```sh
git show 4169cb3b:docs/training.md
```

Historical evidence links in this documentation use fixed revisions. Old
paths mentioned in source comments can also be recovered with `git show`.
The temporary migration ledger and editorial review are available in Hindsight
as `Documentation consolidation migration ledger (26fcbde7)` and
`Documentation consolidation editorial review (26fcbde7)`, and in Git at that
revision. The working tree retains seven short compatibility links for
technical references that now live in the manual.

The manual's `modules.md` is a runtime catalogue; the root `modules.md` is the
original object input-file map. After building the manual, run
`python3 tools/ci/docs_audit.py` to check current repository links, module
coverage and generated links. The audit does not require deleted investigation
files or a repository archive.
