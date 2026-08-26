# slmodemd's headers, verbatim

These seven files are **copied byte for byte from slmodemd** and are **never
edited here**. `tools/vendorcheck.py` enforces both halves of that: the copies
against `tools/vendor.json`'s hashes (always), and upstream against the copies
(when upstream is present).

    source     /home/philpem/dev/sip-D-modem/slmodemd
    licence    BSD 3-clause, Copyright (c) 2002 Smart Link Ltd.
    author     sashak@smlink.com

Upstream is untracked in the parent repository as of `09831d27`, so there is
no commit to pin against — the sha256es in `tools/vendor.json` **are** the pin.
There are three same-named directories under `d-modem/`; the path above is the
one that is upstream, and the manifest records it so that "checked against
upstream" names which upstream.

## Why they are here

`dsplibs.o` is one half of an ABI and slmodemd is the other. `struct dp`,
`struct dp_operations`, `enum DP_ID` and the `DPSTAT_*` codes are not inferred
from the disassembly — they are **stated** by the project the blob links into,
by its authors, in these files. That is evidence rank 1, the same rank as a
`.rodata` format string.

Before this, the tree carried a hand copy and said so in its own comment:

> The originals are in slmodemd/modem_dp.h; they are duplicated here so this
> tree builds standalone.
> — `include/dsplib/dp.h`

A hand copy of a layout is a divergence nothing can see: both halves compile
and every offset in the loser is quietly wrong. That is `onedef.py`'s argument
one repository out, and vendoring is the fix in the same shape — one home.

It also leaves the door open for the thing this reconstruction is ultimately
for: a single tree in which slmodemd and a rebuilt `dsplibs` are compiled
together. Whatever else that costs, it will not cost a reconciliation of two
independently drifted copies of the boundary.

## The rule, and what it implies

**Verbatim means the vendored file is never edited. Every accommodation lands
on our side.** Concretely:

- `modem_dp.h` declares `int (*delete)(struct dp *);`. `delete` is a C++
  keyword, so **these headers cannot be included from a `.cpp`**. No `.cpp` in
  this tree needs them today. If one ever does, the accommodation is a wrapper
  of ours that `#define`s around the keyword and `#undef`s after — not a
  rename in the file below.
- `modem_dp.h` includes `<modem_defs.h>` with angle brackets, so the build
  passes `-Ithird_party/slmodem`. That `-I` is in the flag string in both
  `tools/toolchain/period.mk` and `tools/toolchain/period_inner.sh`.
- `modem_defs.h` pulls `<linux/types.h>` and `<sys/types.h>`. Both are present
  in the period container and on the host; it preprocesses and compiles clean
  standalone under GCC 3.4.2.

## Re-vendoring

Copy all seven again, run `python3 tools/vendorcheck.py --bless`, and say in
the commit what changed and **whether any boundary type moved**. A layout
change upstream is a finding, not a refresh.
