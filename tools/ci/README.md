# Gentoo codegen loss reviews

The period workflow compares the PR's base and head measurements and reports
both lost and gained exact symbols. Numerical regressions remain visible.
`period_metrics.py --check` fails every loss that lacks matching review evidence
or a reviewed displacement. Period differential failures remain hard failures.

`tools/ci/displaced.json` is the bounded-layout ledger shared with
`byteident.py --ratchet`. It does not replace a loss review and is reported
separately. An exact loss is accepted only by an entry whose `metric` is
`exact` or `both` and whose `body_sha256` still matches the worst per-TU head
body. A link-selected loss uses `selected_body_sha256`, the body the
recovered-order link selects; `metric=both` requires both hashes. A
`selected`-only entry can never excuse an exact loss. A name the link defines
once has no independent selected identity, so its selected loss is the same
event as its per-TU loss and the `exact` entry covers it; the selected check
applies to multiply-defined names. The ledger is validated by
`byteident.validate_displaced`, so both tools use one schema.

After reviewing a structural recovery against original instructions, complete-TU
collateral and the period differential gate, record an accepted codegen loss in
`period_reviewed_losses.json`. Each entry requires the full PR base SHA, exact
symbol name, accepted source commit, evidence path/revision and current function
digest. Evidence links pin the documentation revision, which can differ from
the accepted source commit when the review was recorded afterward.
The digest covers actual function bytes and canonical relocation offsets, kinds
and destinations, using `byteident.body`. Missing or multiply defined functions
cannot receive review credit.

Measure the digest from the same Gentoo objects used for the head metrics:

```sh
python3 tools/ci/period_metrics.py --fingerprint V34EchoPreFilter \
    --head-objects build/tc_out
```

Record that output only after reviewing the candidate. A changed body or
relocation, a different base SHA, or another lost symbol requires fresh review.
An entry is inactive when its symbol is no longer lost; it cannot cover another
symbol. The report states reviewed and unreviewed counts over the complete loss
set and links each accepted loss to its evidence. The current entry records the
[reviewed V.34 history-owner refinement](../../docs/method/refinement.md#recover-the-centeradapt-history-owner-before-fitting-its-spill).
Its body pin supersedes the first scratch regression accepted in
`d63428a4c5e81d42c0ed24261e3a8359a5be155e`; that old digest does not authorize
the changed body. The JSON names the current accepted source and evidence
revisions explicitly.

Run the detector's firing and rejection controls without compiling C:

```sh
python3 tools/ci/period_metrics.py --self-test
python3 tools/ci/test_period_metrics.py
```
