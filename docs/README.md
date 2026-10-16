# Documentation

Start with the [Maintainer Manual](maintainer/index.md). It explains modem
operation, ownership, lifecycle and the algorithms used by each module.

| Material | Home | Use |
|---|---|---|
| Module responsibilities and algorithms | [Module catalogue](maintainer/modules.md), [signal paths](maintainer/signal-processing.md), [V.34 training](maintainer/training-algorithms.md) | Trace samples, bits and control through the implementation |
| Configuration and reference | [Configuration](maintainer/configuration.md), [parameters](maintainer/parameters.md), [terminology](maintainer/glossary.md), [call-progress tables](maintainer/call-progress.md) | Interpret fields, units and state transitions |
| Filter design and clock assumptions | [Coefficients](maintainer/coefficients.md), [sample rates](maintainer/sample-rates.md), [resampler banks](maintainer/resampler-banks.md) | Understand the retained tables and the separate retarget work |
| Maintenance procedure | [Maintenance](maintainer/maintenance.md), [reconstruction method](method/README.md) | Establish evidence boundaries and run the deciding gates |
| Current objectives | [Phase ledger and remaining work](remaining.md) | Separate completed source coverage from validation and final-object identity |
| Primary evidence | [Findings](findings.md), [deviations](deviations.md), [attribution](attribution.md), [input modules](modules.md) | Check observations and their original scope |
| Generated measurements and registers | [Coverage](coverage.md), attribution JSON, issue ledgers and the [portability review](tolerance-split-register-review.md) | Reproduce measurements; counts apply to their stated baseline |
| Deployment tools | [Blob fixes](blobfix.md), [hybrid blob builds](forkblob.md), [interop](interop.md) | Use these specific workflows |
| Historical investigations | [History index](history/README.md) | Find bounded experiments, negative results, old plans and supporting evidence |
| Salvaged reference material | [V.92 CP salvage](salvage/v92cp/README.md) | Understand recovered reference material and its limits |

## Where new material belongs

Put explanations of current modules, algorithms, contracts and maintenance
tasks in the manual. Put general reconstruction technique in `method/`.
Keep machine-consumed registers and generated reports at the paths their
tools use. Findings and deviations retain stable identifiers and their
correction history.

Put dated experiments and completed plans in the appropriate `history/`
subdirectory. Record the baseline, boundary, controls, measured denominators
and unresolved question. Save durable decisions and negative results to
Hindsight with their source path and revision so later investigations can
find them. Hindsight supports recall; repository evidence remains available
for review and reproduction without the service.

## Consolidation record

The 16 October 2026 consolidation starts from upstream `4169cb3b`. The
[migration ledger](history/migration.tsv) accounts for all 722 moved root
documents: seven technical references entered the manual and 715 investigation
records entered the archive and Hindsight. Original versions of the seven
references remain in `history/reference/`; current manual corrections are
separate from those historical measurements.

Compatibility pointers retain 33 original paths cited by source comments or
tools. New documentation should link to the destination directly. The
findings, deviations, phase ledger, generated attribution and tool-consumed
registers keep their established paths. The manual's `modules.md` is a runtime
catalogue; the root `modules.md` is the original object input-file map.

After building the manual, run `python3 tools/ci/docs_audit.py` to check current
repository links, module coverage, archive preservation and generated links.
The [consolidation review](history/project/documentation-consolidation.md)
records the editorial corrections and validation scope.
