# Unsigned wrapper arithmetic and fragment reloads

`dp_wrapper_run` now uses unsigned minima and ring remainders and reads the
current fragment value after each copy. Its `host_frag` field has the unsigned
32-bit type supported by the original fragment comparison. The public caller
count remains signed, including its positive-count loop guard. Layout and API
argument widths are unchanged.

These are partial source recoveries. The original has three JBE minima, two
unsigned DIV operations and one JB fragment comparison, plus a fragment load
between each external copy and its remainder calculation. The previous source
emitted signed minima/IDIV and cached the span at entry. The replacement follows
the observed operations; it does not recover the complete function or uniquely
prove original syntax or field spelling.

## Prior result and bounded validation

F11831 measured these operations at `e0052eec`; the durable domain/results live
in commit `158dbfafe`. Its seven full-TU controls were withheld under the old
complete-exact-gain criterion, with no runtime, fuzz or mutation execution and
no recorded contradictory native evidence. This review runs only baseline and
the prior final unsigned-fragment endpoint. No type, register, declaration or
compiler-profile palette was reopened.

The source pin is `dfc23b7d1`, including the validated V32 count recovery.
`tools/recovery_dp_wrapper_unsigned.py` uses immutable pre-adoption objects and
configuration under `build/dp-wrapper-unsigned293/frozen`, records their hashes
in `manifest.json`, and reads source and headers from Git. Historical headers
are necessary because production now contains the unsigned field. Switching
from identical live to historical header inputs reproduced all 18 objects raw.
A fresh-output replay after production header adoption also reproduces all 18
objects raw, using the archived baseline rather than current production.

```sh
python3 tools/recovery_dp_wrapper_unsigned.py
python3 tools/recovery_dp_wrapper_unsigned.py --audit
python3 tools/recovery_dp_wrapper_unsigned.py --period
python3 tools/recovery_dp_wrapper_unsigned.py \
  --out build/dp-wrapper-unsigned293-fresh \
  --baseline-dir build/dp-wrapper-unsigned293/frozen
```

The actual final-cell compiler command is:

```sh
cd /work/dp_wrapper/unsigned-fragment && export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; exec gcc -c -I/work/dp_wrapper/unsigned-fragment/include -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -I/work/historical-headers-dfc23b7d1e92f5913b22b124771211d3e4650771/include -D__SIZEOF_POINTER__=4 -include /work/historical-headers-dfc23b7d1e92f5913b22b124771211d3e4650771/tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS -DDSPLIB_REPRODUCE_BUGS -v -save-temps -da -dP -DDSPLIB_REPRODUCE_BUGS -o /work/dp_wrapper/unsigned-fragment/candidate.o /work/dp_wrapper/unsigned-fragment/dp_wrapper.c
```

The ledger records the full Docker argv, complete Gentoo GCC 3.4.2-r2 flags,
assembler identity, final reproduction define, source/header/object hashes and
all body verdicts. No host compiler dump participates.

## Header consumers and collateral

The transitive production include graph contains nine TUs: `dp_init.c`,
`dp_wrapper.c`, B103/V22/V23/V32 lowercase C glue, and `V32int.c`, `V32mod.c`,
`V32stc.c`. All nine baseline objects raw-reproduce the frozen validated source.
All eight non-wrapper header consumers remain raw-identical after the header
overlay. The two cells produce 110 body verdicts, 55 per cell; 30 exact entries
remain exact with no gains or losses. Data, BSS, public bindings and resolved
relocation consumers are unchanged across every pair.

| Wrapper function | Native bytes | Baseline bytes | Final bytes | Final verdict |
|---|---:|---:|---:|---|
| create | 694 | 692 | 692 | SIZE 2 |
| run | 575 | 573 | 580 | SIZE 5 |
| delete | 85 | 85 | 85 | EXACT |

Only `run` changes. Its length gap worsens from two to five bytes; these are
length differences, not differing-byte counts. Independent opcode and read-age
evidence supports the recovery despite that distance. One final division uses
a memory-form DIVL instead of register DIV; both perform unsigned 32-bit division.

The witness detector records both post-copy fragment loads and division windows,
three unsigned minima and the unsigned fragment branch. It accepts native and
final and refuses the signed/cached baseline as a known control. It does not
establish arbitrary overlapping buffers or callbacks that modify fragment state.

## Period result and production boundary

Baseline and final each pass the existing `t_dp_wrapper` fixture, 33,686 checks.
The fixture creates real wrappers around synthetic datapump callbacks, covers
equal-rate and supported rate bridges, ragged chunks, wrap boundaries and status
propagation, and compares both output and callback logs. It is a component
lifecycle test, not modem end-to-end evidence. No fabricated negative fragment,
high-bit field or overlap history was added.

The fixture was also compiled with each cell's matching header; both fixture
objects raw-match the existing period fixture. All nine selected source objects
are checked against their hashes after the isolated gate. The production focused
run passes one fixture and 33,686 checks, and all nine production period objects
raw-match the audited final endpoint (`production-audit.json`). Main source and
header are frozen for the parent's full integration gate. No mutation campaign
was added; no registered mutation row targets these source/header paths.
