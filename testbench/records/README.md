# Bench records

Tracked. This is the half of a bench run that is **not** a reading.

`captures/` is gitignored, on the stated principle that the repository keeps
the instruments and not the readings: the analysis scripts regenerate every
number from the audio, so the audio is reproducible input. That is right, and
it stays.

It is wrong for three kinds of file, which is why this directory exists:

* **Pre-registrations.** `ab149-PREREG.txt` fixes the outcome, the window and
  the exclusion rule *before* the data exists. Its whole value is that it
  cannot have been written afterwards, and a record that lives in an ignored
  directory cannot demonstrate that. It is the difference between a result and
  a story.
* **Scored results.** `ab149-RESULT.txt` is what the pre-registered scorer
  printed, kept verbatim so a later reader can check that the numbers quoted
  in `docs/findings.md` are the numbers the tool produced.
* **Discard notices.** `pab-DISCARDED.txt` says why sixteen calls were thrown
  away. A batch that vanishes with no note looks like a batch that never
  happened, and the next person re-runs it into the same wall.

**How it was handled before, and why that was fragile.** Three such files were
force-added into `captures/` past the ignore rule — `pab-DISCARDED.txt` and two
`preemph-ab*-ANALYSIS-PLAN.md`. That works, and those citations do resolve in a
clone. But it depends on someone remembering `git add -f` for exactly the files
that matter, inside a directory whose whole purpose is to be discarded, and
nothing enforces or even hints at it. A directory that is tracked by default
needs no one to remember anything.

The two `preemph-ab*-ANALYSIS-PLAN.md` files stay in `captures/` regardless:
`claude_re_v34_review/docs/findings.md` — a separate repository — cites them by
that path, and moving them would break a record this tree does not own.

## What is NOT here

The audio and the per-call logs. Those go to the ZFS store via
`testbench/archive.py`, which keeps only captures a finding or a tool actually
names, in `<family>/<prefix>/` subdirectories, and writes `INDEX.tsv`.

`archive-INDEX.tsv` in this directory is a copy of that index, tracked, so the
tree always knows what is in the store — which captures, how big, and which
document cites each one — without needing the store mounted.

    testbench/archive.py --dry-run              # what would be kept, and why
    testbench/archive.py --also 'ab149-*'       # keep a batch cited as a set
