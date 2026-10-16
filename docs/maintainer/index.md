# dsplibs maintainer guide

`dsplibs` is the signal-processing library behind the Smart Link software
modem. It turns host payload bits into telephone-line samples and recovers
bits from received samples. This tree reconstructs the source of the original
32-bit `dsplibs.o`, including call setup, data modems, fax and voice services.

This guide is for maintainers tracing a call, diagnosing a datapump, or
changing a recovered layout. It assumes familiarity with C and C++; the
[signal-processing chapter](signal-processing.md) introduces the modem terms
needed to follow the code.

Start with [architecture and ownership](architecture.md), then follow
[a call from dialling to data](lifecycle.md). Use
[maintenance](maintenance.md) for the evidence and gates a change requires.
[Reconstruction debt](reconstruction-debt.md) records concrete structural gaps
and misleading documentation found while preparing this guide.

## What “reconstructed” establishes

The project's [phase ledger](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/remaining.md)
records completion of source reconstruction, including fax. That establishes
source coverage; it does not establish complete structural understanding,
complete reachable-path testing, or an identical final object. These are
separate objectives. For current counts, run the measurement tools described
in [maintenance](maintenance.md#measure-the-claim-you-are-making); historical
counts in findings and comments are dated evidence.

The original object decides compatibility. Standards explain the protocol and
provide independent correctness checks, but a standards citation alone cannot
justify changing reconstructed behaviour. See
[standards and references](references.md) for editions and their limits.

## Build and read the guide

From the repository root, with Python 3.12 and its `venv` support available:

```sh
python3 -m venv /tmp/dsplibs-docs-venv
/tmp/dsplibs-docs-venv/bin/python -m pip install -r tools/ci/docs-requirements.txt
/tmp/dsplibs-docs-venv/bin/python -m mkdocs build --strict
/tmp/dsplibs-docs-venv/bin/python -m mkdocs serve
```

The build writes `build/docs-site/`; the preview normally listens at
`http://127.0.0.1:8000/`. The configuration uses
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/getting-started/).
The documentation workflow checks the build and uploads the generated site
as a PR workflow artifact. Hosting is a separate repository decision.

The curated guide lives in `docs/maintainer/`. Existing findings, experiments
and registers stay at their established paths under `docs/`, linked from the
guide through GitHub. This keeps historical investigation notes available
without presenting every experiment as current operating guidance. Repository
links follow `master`; use Git history when reading this guide at an older
revision.
