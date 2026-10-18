# V34 snapshot index recovery

The snapshot helper's two six-entry loops now use a signed short index.
The native handshake narrows each increment and compares the low word;
the reconstruction previously used `int`. This is a supported partial
source recovery. It adds no byte-exact function.

## Evidence and scope

`tx1_ts_snapshot` in `include/dsplib/v34hstx1_arms.h` first sums six predictor
values, then bit-reverses six retained coefficient words. The native first
loop sign-extends BP to ECX at 0x672ed and compares CX with five at 0x672fa.
The second loop sign-extends AX at 0x67473 and compares AX with five at
0x67474. Both loop indices run from zero through five on ordinary inputs;
no negative-index or overflow fixture is needed to establish those native
operations. The arithmetic temporary `t` remains `int`.

F11912 and `tools/gcc3_v34_snapshot_width.py` previously measured the index
and temporary widths at `7edfb734`. The index recovered native word operations
but did not make the complete handshake exact. The present experiment starts
from `dfc23b7d1`, including the current timing helper graph, and tests only
the independently witnessed index change. No temporary-width, register,
declaration-order, inlining-option or forced-inline control is added.

There are exactly two direct header consumers: production
`src/pump/v34/V34hshak.c` and fixture `test/unit/t_v34hstx1.c`. No header
includes the arms header, so there is no additional transitive consumer.
Both consumers receive the same selected header in the replay and in the
production period run. The replay also inventories all 35 emitted fixture
bodies: six change, including its snapshot parent and caller/layout effects.
Those measurement-apparatus bodies have no native-equivalence claim; the
production audit below separately accounts for all 53 reconstruction bodies.

## Complete emitted-body audit

The replay compares all 53 emitted functions, including 24 without a
same-named native counterpart. Its 29 native-common functions retain all nine
exact matches, with no exact gain or loss.

| Body | Baseline | Selected | Interpretation |
| --- | ---: | ---: | --- |
| `v34tx1_trnseg4a` | 3676 bytes | 3700 bytes | Contains the inlined snapshot helper |
| `v34handshak` | 12867 bytes | 12867 bytes | One changed call-displacement byte |
| Other 51 emitted bodies | unchanged | unchanged | Bytes and canonical relocations agree |

Both `00.cgraph` dumps explicitly inline `tx1_ts_snapshot` into
`v34tx1_trnseg4a`. The parent body's first RTL already adds the signed-word
index extensions and word comparisons. Thus its change is the intended
snapshot change, rather than unexplained collateral in an unrelated arm.
The dispatcher calls that same parent in both endpoints. The replay decodes
the signed PC-relative displacement, proves the destination remains
`v34tx1_trnseg4a`, and verifies all dispatcher bytes agree after normalising
only those four proven call-displacement bytes. One of those four bytes
actually differs because text placement changes. The native whole handshake
remains 61541 bytes versus 12867, a SIZE(48674) gap; this experiment does not
resolve the larger outlining question.

Raw `.rodata` bytes and text-section relocation addends change with function
placement. They are not described as raw-identical. Every affected relocation
resolves to the same owning function and interior offset; all nonrelocation
bytes agree. Nontext section sizes/alignment/flags, strong exports and resolved
imports remain unchanged. Other nontext sections remain raw-identical.
The report preserves both raw metadata ledgers and the canonical destination
ledger, rather than masking unknown jump-table destinations.

## Reproduction and validation

`tools/recovery_v34_snapshot_index.py` pins source and header to `dfc23b7d1`,
requires literal reproduction of the frozen canonical object, and checks that
the selected header equals the adopted header. It records complete commands,
source/header/object hashes, actual Gentoo GCC 3.4.2-r2 and selected assembler
identity, compiler dumps and every emitted-body verdict. The complete frozen
flags and the shared helper's final mandatory `DSPLIB_REPRODUCE_BUGS` define
are used unchanged. The baseline object SHA-256 is
`31b758e25984e680353a7eb0fb159f26e4a879c82c6bac723f78ee368d49085e`.

The initial build-only period probe used separate output storage and the
candidate source and direct-header fixture objects. After approval, the real
header was changed and a normal production period run passed all three
fixtures with zero failures: `t_v34hstx1`, `t_v34hshak`, `t_v34hsfsk`.
The table-1 fixture reports 25940 checks; the FSK gate reports 3795. Existing
handshake state, bulk-delay and bit-transfer coverage also passes. No fixture
input, assertion or tolerance changed, and no mutation was executed.

The production source object is raw-identical to the selected comparison
object. The fixture replay also uses the production relative input filename:
an initial absolute filename changed its `__FILE__` string and corresponding
literal references. That apparatus difference was preserved, identified and
corrected before the final fixture raw comparison. It is not a source or
functional discrepancy. The parent performs the complete phase gate.

```sh
python3 tools/recovery_v34_snapshot_index.py \
  --objects build/snapshot-dfc-baseline --out /tmp/v34-snapshot-index
make period T='t_v34hstx1 t_v34hshak t_v34hsfsk' J=8
```
