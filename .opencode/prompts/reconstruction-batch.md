---
description: Run one reconstruction batch on dsplibs.o — derive the mechanism first, then confirm; verify delegated work; record and report. Cold-startable standing prompt.
---

Continue the `slmodemd/dsplibs.o` reconstruction in this repository. Read this
whole prompt before acting; it is written to be self-contained.

Priorities, in order: **source reconstruction errors before compiler-induced
ones**; **control-flow (branch/cfg) differences before allocation-only ones**;
**whole-function promotions (EXACT/REGALLOC) over partial byte progress**. Keep
working batches until you need input, posting a status update at the end of each
batch and then starting the next. If you find a process speedup, say so and apply
it.

## 0. Orient (cold start)

1. Read `AGENTS.md` (a symlink to `CLAUDE.md`) — it is the authority for
   boundaries, validation and records. Then `scratch/HANDOVER-2026-11-15.md`
   (the running session log; find its latest `## Session update N`), then
   `docs/method/README.md` and only the references the current inquiry needs.
2. `docs/findings.md` holds the numbered findings (FNNNNN). Read the most recent
   ones and search it (`grep -n "^## F"`) before re-investigating anything —
   many targets are already closed or parked with a named discriminator.
3. `docs/method/refinement.md` is the **Playbook** (levers; numbering is not
   contiguous — `grep -n "^#\{2,3\} Lever"` before adding one, and take the next
   free number). Levers 28-43 are worked derivations; start there.
4. The extracted GCC 3.4.2 source is at
   `/tmp/opencode/gccsrc/gcc-3.4.2/gcc/` (`ifcvt.c`, `i386.c`, `fold-const.c`,
   `global.c`, `local-alloc.c`, `predict.c`, `bb-reorder.c`, `recog.c`,
   `loop-unswitch.c`, `regmove.c`, `cfgcleanup.c`). Read it, do not guess.

Do NOT read, search, import or reference anything under `re/` — it belongs to
another effort.

## 1. Method gate — DERIVE, DO NOT ENUMERATE

The easy wins are gone; the residue is compiler-internal. **Enumerating spellings
is not a method** — a sweep is evidence that the target is un-derived. Every
target's first deliverable is a **mechanism statement**, not a variant table:

- **Locate the pass** that produces the difference and quote its decision
  predicate and the inputs it reads, with `file:line` into the extracted tree.
  If you cannot, record that and why — that is the finding.
- **Derive the forced shape:** which source forms can and cannot produce the
  object's bytes, from that predicate.
- **State the impulse, then the inert directions.** Which predicate inputs a
  source change can actually move, and which are invariants. Example: `global.c`'s
  `allocno_compare` priority reads `(n_refs, freq, live_length, size)` and
  `floor_log2(n_refs)` is a step function — a change that does not cross a
  threshold moves nothing, so do not sample it. `find_reg` then takes the first
  free register in `reg_alloc_order` (identity on i386), so an allocno's colour is
  computable from those four quantities.
- **Predict the consequence and name the falsifier** before compiling.

Then confirm with **at most three** variants of the derived shape.
`tools/toolchain/variant_run.py` is the harness (`--spec SPEC.json [--jobs 8]`;
spec: `source`, `symbols`, optional `base`/`extra_flags`, and `variants` as
`{name, edits:[[find,replace,("all")?]], extra_flags?}`. It never writes to the
tree, reads the real per-TU flags from `make -n`, marks an absent `find` INVALID,
and prints `CONTROL: baseline reproduces the in-tree object: OK/FAIL`, exit 2 on
failure. Read the lane-free `dmn` column before the byte delta.)

**Acceptance is a confirmed prediction, not an improved metric.** A narrowing byte
delta with no named mechanism is an unattributed change: record it as one, and
keep it only if a mechanism also supports it. `symdiff` in this repo is the
multiset **multiplicity** sum.

**Type every negative:** excluded by a derived predicate (a real domain, with its
scope); sampled and inert (no information); or mechanism unnamed (parked, with the
missing measurement or compiler fact named). Never present the second or third as
the first. Two batches without a new explanatory observation need a
hypothesis/scope review, not more synonyms.

Where the quantity you need is not printed, **build the instrument** — a dump
reader, an allocno-order computation, a pass diff.

Useful apparatus (each honours the real per-TU build):
- build one object (goal MUST be absolute; `rm` first or make may skip on an mtime tie):
  `rm -f "$PWD/build/tc_out/<path with / -> _>.o" && make -f tools/toolchain/period.mk "$PWD/build/tc_out/<path>.o"`
- grade: `python3 tools/toolchain/byteident.py --why <symbol>` (grade, `delta N`,
  `longest contiguous instruction run: N / M`, candidate count)
- reference bytes: `objdump -d --disassemble=<symbol> ref/slmodemd/dsplibs.o`
  (if the output starts with section headers the symbol was not found)
- screens (gitignored, `scratch/cd/`): `setcc.py <census.json>` (read the SIGN: a
  NEGATIVE `d` means the object masks where we branch; POSITIVE the reverse),
  `cfgdiff.py <census.json> [all]` (branch-class/CFG), `whatdiff.py <sym>`,
  `align.py <sym>`, `onerow.py`
- per-pass dumps of OUR build: `docker run --rm --platform linux/386 -v "$PWD:/src:ro" -v "/tmp/opencode/<dir>:/out" -w /out ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest gcc -c <period flags> -da -o /out/x.o /src/<file>` (dumps land as `<file>.c.<NN>.<pass>`; `-fdump-rtl-all` does not exist in 3.4). The ORIGINAL's intermediate RTL is unrecoverable — the compiler source plus the final bytes are the evidence.

## 2. Choose a batch

Take a coherent queue (one screen output, or one pass/mechanism family), normally
one complete function per worker. A batch limit is a ceiling, not a target — do
not fill it with exhausted or weaker leads. State each target's closure unit.

## 3. Dispatch (when delegation helps)

Two or three workers maximum (more rarely pays for itself). Give each a compact
packet: the repo path, the hard rules, the derive-first gate above, its exact
targets (with the source path and object name taken from the census
`function_fidelity[SYM]["object"]` key — do not guess the file), the harness
commands, the fixture check, and the requirement to **write a JSON report file**
(schema: a list of `{symbol, object, source, md5, delta, candidate_bytes, insns,
run:"N/M", symdiff, setcc_ref, setcc_our, retained, fixtures, note}`; for a
not-retained target set `md5` to the md5 as found). Tell each worker to read
`git diff <file>` first and preserve existing retained hunks, and to never touch
another worker's files.

## 4. Verify independently — always

A worker's result message can be lost or blank while its edits are already in the
tree (this has happened repeatedly). After every dispatch:

- `git status --short src/ include/` and `git diff` — find edits with no claim.
- `python3 tools/toolchain/verify_report.py REPORT.json [REPORT2.json ...]` —
  re-derives every claim (rebuild with `rm`, grade, both `setcc` forms, symdiff)
  and rechecks md5s. It recomputes rather than trusts.
- For anything retained, re-measure it yourself and prove behaviour-preservation
  from the diff before keeping it. A metric gain alone is not a reason to keep a
  change that no mechanism supports.
- Rebuild EVERY affected object before measuring; a stale object has produced
  false "unchanged" readings. Rebuild after restoring a source file too.

## 5. Validate and record

- Period differential (required for anything retained):
  `grep -rln <SymbolFragment> test/unit/` then `make period T=<fixture>` — expect
  `period differential: 1 passed, 0 failed`. Confirm the fixture name exists: a
  wrong name exits 1 with "unknown test", which is a selection error, not a
  regression.
- Grade movement: run the full census (`python3 tools/toolchain/byteident.py
  --json-out /tmp/opencode/census_<tag>.json`) at **checkpoints**, not per batch,
  and compare against the previous JSON (top-level `exact`/`regalloc`/`nonmatch`/
  `unresolved`, and the per-symbol `byte_delta` moves). NOTE: the top-level
  `size`/`bytes` keys are FIDELITY BUCKET counts — a row moving `size -> bytes` is
  progress (its size converged), not a loss.
- Record: append `## FNNNNN` to `docs/findings.md` (witness bytes, mechanism with
  compiler file:line, measured tables, retained source, recorded costs, named
  residuals with reopening discriminators); add or extend a Playbook lever in
  `docs/method/refinement.md` for every new **mechanism**; append
  `## Session update N` to `scratch/HANDOVER-2026-11-15.md`. Keep records honest:
  state recorded costs, rejected variants and losses.
- `python3 tools/refcheck.py` must pass, and `git diff --check` must be clean
  (use TAB-indented code blocks in docs — a 4-space Markdown indent wrapping
  tab-indented C trips "space before tab in indent").

## 6. Hard rules

- Never revert, stash, reset or `checkout` other workers' work. Work in an owned
  worktree; preserve unfamiliar changes.
- Never commit, amend, push or open a PR unless explicitly asked.
- Do not weaken the period oracle, widen tolerances, add allow-lists, or edit
  source to fit a fixture. Preserve original behaviour, including original bugs.
- Do not add attributes, asm, volatile barriers, padding or artificial
  temporaries to force codegen.
- Recover the original call/inline boundaries; where a body is repeated, compare
  it with existing helpers before rebuilding it by hand.
- The complete TU, in original function order, is the recovery boundary: a change
  can move registers in later functions even when their source is unchanged.
  Re-grade every emitted function in an affected TU, and re-grade every function
  that inlines a header you changed.
- If a target resists a derived attempt, park it precisely — with the missing
  measurement or compiler fact that would unlock it — rather than listing failed
  spellings. Record the reopening condition.
- The owner's escalation rule: if a function resists both a normal and an
  extra-high-effort investigation, escalate to the configured Astra path with a
  frozen evidence packet before calling it an allocation boundary.

## 7. Status update (end of every batch)

Report, concisely: the batch's targets; per target the grade/delta/run/symdiff/
`setcc` before and after; **the mechanism named, with its compiler file:line**
(or "mechanism unnamed — parked"); what was retained and what was refused with the
reason; recorded costs; the census line (EXACT/REGALLOC/NONMATCH/UNRESOLVED and
denominators); the fixtures run with their exit status; the tree state (modified
files, nothing committed); and the next batch's queue.
