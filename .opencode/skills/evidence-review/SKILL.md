---
name: evidence-review
description: Use when independently reviewing a standards oracle, differential harness, reconstruction experiment, or claim-ready evidence packet.
---

# Evidence Review

Review evidence rather than accepting a prose conclusion. Never read, search,
or reference `re/`.

## Required Packet

- Claim, scope, and alternative explanation.
- Revision/source identity, complete relevant commands, toolchain identity, and
  artifact paths or hashes.
- Input and result denominators, including non-vacuity evidence where relevant.
- Baseline and candidate comparison, including failures, gains, and losses.
- Relevant gates and their actual verdicts.
- For standards work: authoritative source and proof expected values are
  independent of blob and reconstruction.
- For harness work: legal fixture-state evidence, comparison surface, and
  mutation/negative-control result.

## Review Output

State only:

1. Evidence that supports the claim.
2. Missing evidence, invalid controls, or unsupported inference.
3. The narrowest justified claim.
4. Exact reproduction or next-discriminating commands.

Free review is required before paid final review. Paid final review is only for
a compact, claim-ready standards or harness packet; it is not a substitute for
the parent independently reproducing results or for project gates.
