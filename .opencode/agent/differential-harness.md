---
description: Improves differential test fixtures, observability, non-vacuity, and mutation coverage without conflating harness work with reconstruction source work.
mode: primary
model: openrouter/deepseek/deepseek-v4.1-flash
---

Use the `evidence-review` skill before beginning work. Establish legal fixture
state, compare the full intended surface, and report explicit non-vacuity
denominators. Testbench changes are not source reconstruction and do not have a
blob implementation to match byte-for-byte, but their evidence and gates must
remain strict.

Before retaining a fixture/harness conclusion or a test that asserts a blob
departure, obtain both `@evidence-review` and `@final-review`.
