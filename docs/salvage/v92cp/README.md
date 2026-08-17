# Salvage: V.92 CP bit-level packing

**THIS IS NOT RECONSTRUCTION SOURCE. It is evidence, in the way a disassembly
listing is evidence.** Nothing here is built, nothing here has seen GCC 3.4.2,
and nothing here has run under `make phase`. It lives under `docs/` and not
`src/` for exactly that reason. Do not move a line of it into `src/` without
taking it through the differential tier like everything else.

## What it is

Three files, verbatim and unmodified, from `re/` -- an earlier
reverse-engineering effort against this same blob, which is being deleted. They
are the one thing in it that this tree does not already have, equally well or
better. Findings 4300 (the full disposition, 100 claims bucketed) and 4301 (why
these three files and nothing else) are the record.

| file | lines | what |
|---|--:|---|
| `sl_v92_cp.c` | 829 | the recovered packing/unpacking, descriptor masks, group compaction, CRC/padding tail |
| `sl_v92_cp.h` | 135 | its layout constants and prototypes |
| `v92_cp_probe.c` | 795 | the blob-linked probe that proves it |

## Why it is worth keeping

`src/pump/v90/V92CP.cpp` is 68 lines -- the constructor and the destructor, two
of the class's twelve symbols. `V92CP::infoToBits` is called from **fifteen
sites** in the object's `.text` and is unwritten here with no recorded reason;
finding 3520, which looks like it rules on this, is about `bitsToInfo` and is
correct about that symbol only. See 4301 for the relocation counts with their
controls.

`v92_cp_probe.c` declares the blob's real mangled symbols as `__asm__` aliases,
links against the object, and compares with **`memcmp`, no tolerance**. That
matters: the rest of `re/` compares floats with `fabsf(a-b) <= 1e-6`, which this
project does not accept and which is why nothing else from that tree survived
review. This probe compares bytes, so its results stand on their own terms.

## What it does not establish

`re/`'s own README: "the remaining V92CP work is broadening any unobserved
`infoToBits()` states." The probe exercises the states it managed to construct,
not the states a live V.92 session produces. One of its fifteen claims -- the
`setV92CPpckFromParamsInfo` field mapping, `info_byte = info_source - 8` / `- 20`
-- `re/` itself labels modelled rather than recovered.

Treat every offset and rule in here as **a strong lead to verify**, not as a
result already banked. The verification is a differential test against the blob,
here, in `test/unit/`.

## When this directory should disappear

When Phase 3 (task #122) writes `V92CP` properly and the differential tier
covers `infoToBits`. At that point this is superseded by something gated, and
keeping it would only give a future reader two sources of truth. Delete it then,
and say in the commit which finding replaced it.
