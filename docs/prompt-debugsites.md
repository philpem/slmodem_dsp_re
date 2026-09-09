# Prompt: restore the missing diagnostic call sites

Paste this into the other session, or hand it to an agent. It is self-contained.

---

Work in `/home/philpem/dev/sip-D-modem/claude_re`. Read `CLAUDE.md` first; the
rules there are not negotiable, in particular that **nothing is committed that
has not passed a differential test**, and that `re/` does not exist to you.

## The job

`python3 tools/debugaudit.py --missing` lists diagnostic call sites that the
blob has and our reconstruction does not, **in functions that are already
reconstructed**. The level ships at zero, so a missing call and a present one
behave identically and no test can see the difference (finding F134). That is
exactly why they have been skipped, and why 731 of them have accumulated.

Restore them, highest count first:

    261  v34handshak         src/pump/v34/V34hshak.c
     28  probeselect         src/pump/v34/V34hshak.c
     15  CALLPROG_Progress   src/callprog/Callprog.c
     12  DialerProgress      src/dialer/Dialer.c
     10  v8handshak          src/v8/v8handshak.c
      9  v34handshakinit     src/pump/v34/V34hshak.c

## Do `probeselect` FIRST, out of count order

It is 28 of 43 rather than 261 of 262, so it is the tractable one — and two
open investigations are blocked on it specifically (findings F1974, F1976; tasks
#169 and #170).

`probeselect` builds the message this modem SENDS: the header says *"Writes the
rate config at +0xaa84 and the outgoing message at +0xa9ac, and nothing else"*.
It writes a power-reduction request, the offered or chosen symbol rates, and a
pre-emphasis index per rate. **It appears to write no trellis code, no
non-linear-encoder bit and no shaping bit** — and the open question is whether
those are written elsewhere in `v34handshak` or never written at all, in which
case they carry whatever the buffer held.

The blob narrates its own message construction through these very call sites.
Restoring them is what makes that question answerable, so **prefer the sites
that print message fields, chosen rates, or the probe's intermediate values**
over any others in that function.

## How

- `tools/relocscan.py --at .rodata.str1.1:0xNNNN` answers "who references this
  string". Searching the disassembly for a string's address finds nothing and
  proves nothing — the reference is an `R_386_32` against the SECTION symbol
  with the offset as an inline addend. Finding F604, where not knowing this had
  a defect misdiagnosed for weeks.
- The format string and its arguments must be the blob's, in the blob's order.
  A site that prints the right words with the wrong values is worse than a
  missing one.
- `docs/invented_strings.txt` is the register for any string that is OURS and
  not the blob's. If you add a diagnostic the blob does not have, it goes there.
- `make one T=<suite>` is the fast loop; `make phase` before every commit.

## What "done" looks like

- `debugaudit.py --missing` count drops, and `make phase` stays green —
  `period differential: 185 passed, 0 failed` and `phase boundary: ... all OK`.
- `python3 tools/refcheck.py` clean.
- Findings appended for anything the restored diagnostics REVEAL. Numbering:
  this branch used 1904–1975 and master is at 1980+ and above 2300 — **check
  every branch before claiming a block**, numbers have collided eight times.

## The trap that has already been paid for

**A tool that prints nothing is indistinguishable from a tool that is broken.**
`extcheck` printed "(none)" through four broken versions. If you add or change
a detector, show it firing on a known input before trusting a clean run.

Same for the diagnostics themselves: after restoring a site, run something that
should hit it and confirm the line appears. `DSPLIB_DEBUG_ON()` gates them, and
`tools/hybrid_link.sh` puts our datapump into a real slmodemd if you need a live
call — but most sites can be reached from `test/unit/` without one.
