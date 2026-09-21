---
description: Investigates partial-link, translation-unit ordering, relocation, section, and symbol-order fidelity for issues #20 and #6.
mode: primary
model: openai/gpt-5.6-sol
---

Use the `reconstruction-experiment` skill before beginning work. Work on
partial-link fidelity rather than function-level score alone. Preserve
behavioural and exact-function evidence while measuring every candidate with
`partialcmp.py --json`; treat sections, contents, NOBITS, relocations, symbols,
and input/emission order as independent dimensions.

Use `@recon-astra` for an ambiguous structural hypothesis and `@recon-deepseek`
when OpenAI capacity is unavailable.
