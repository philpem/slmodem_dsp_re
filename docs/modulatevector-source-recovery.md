# Native source recovery for `modulatevector`

The recovery restores three source properties witnessed in
`ref/slmodemd/dsplibs.o`: signed-short bisection state, capture of mapper
configuration after `getFrame`, and opposing table pointers whose difference
supplies each peeled sub-index. The baseline is `cc6377b2c`.

This reopens a bounded source-reconstruction task within the earlier
arrangement-only classification. It establishes neither absent algorithm
blocks nor a production semantic failure in the baseline.

## Native witnesses

| Property | Original instructions | Source recovery |
|---|---|---|
| Search state | `0x59f5c..0x59f99`: signed divide-by-two correction (`shr $31`, `add`, `sar $1`), midpoint narrowing and signed-short counter comparison | `lo`, `hi`, `mid` and `i` are `short`; table comparisons remain unsigned |
| Capture timing | Call to `getFrame` at `0x59f0d`, followed by count load at `0x59f12` and index-width load at `0x59f26` | Capture both fields after the call |
| Peeling | `0x59fc4..0x5a03a`, `0x5a079..0x5a0e1`, and `0x5a124..0x5a16c`: opposing short pointers, then pointer subtraction, division by two and subtraction of one | Three ordinary C pointer walks recover the sub-indices; the generalized counter-based `shell_peel` helper is removed |

The field addresses remain unchanged. `tx` is based at object `+0x1be0`;
count and index width are shell `+0xa12` and `+0xa14`, giving object
`+0x25f2` and `+0x25f4`. Tables `t1`, `t2` and `t3` remain shell `+0xa48`,
`+0xb48` and `+0xc48`, giving object `+0x2628`, `+0x2728` and `+0x2828`.
Table entries remain 16-bit zero-extended values and subtraction retains all
32 bits before testing the sign bit.

`getFrame` does not directly assign count or index width. It invokes stored
scrambler callbacks with the whole object, so moving the captures preserves
the native call boundary. The existing real scramblers do not establish a
production case in which these two fields change during that call.

The historical signed-division finding near line 36024 of `docs/findings.md`
describes the baseline midpoint expressions as signed-int division. The
baseline declarations are unsigned, so those expressions actually divide
unsigned values. Narrowing their result afterwards does not restore the
native signed division operation.

## Compiler measurements

All compiled controls use the default `tools/toolchain/period.mk` flags and
the Gentoo GCC 3.4.2-r2 image. Every measured object carries:

```text
GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
```

Instruction counts strip alignment padding. LCS is the register-canonicalised
ordered instruction subsequence reported by `structsim.py`; it does not prove
equivalence. The original has 3,388 bytes and 807 instructions.

| Independent control | Bytes | Instructions | Longest run | LCS / 807 |
|---|---:|---:|---:|---:|
| Rebuilt baseline | 2,996 | 725 | 14 | 281 |
| Capture count and width after `getFrame` | 2,996 | 725 | 14 | 282 |
| Signed-short bisection | 3,028 | 736 | 14 | 293 |
| Pointer-based peeling helper | 2,923 | 712 | 14 | 298 |
| Retained combined source, with three explicit pointer blocks | 2,947 | 721 | 14 | 317 |

The retained function remains `SIZE`. Its byte deficit increases from 392 to
441 bytes, while the ordered instruction correspondence improves. Acceptance
rests on the specific native source witnesses and exact differential results;
this is a partial source recovery, with no claim of improved byte identity.
Each independent control and the retained source preserve the other twelve
defined functions' bodies and relocation records in this translation unit.

The complete Gentoo build produces 300 objects from 300 sources with zero
failures. Fresh baseline and recovered `byteident.py --list-exact` outputs
are identical: **1,143 EXACT, 35 REGALLOC, 84 BYTES and 590 SIZE**, over 1,852
shared symbols. No EXACT symbols are gained or lost.

A further signed-short output-cursor control was declined. It folded the
native zero-extending load followed by signed consumption into a
sign-extending memory load and supplied no stronger reconstruction result.

## Differential boundary

The existing `t_v34shell` fixture passes on both the baseline and recovered
source under Gentoo GCC 3.4.2-r2: one selected binary passed, none failed.
Its mapper section performs **1,327,104 exact checks**.

That section creates 48 component histories: six initialised baud/rate/depth
configurations, two roles, two nonlinear-encoder settings and two bit sources.
Each history calls `modulatevector` 48 times, for 2,304 calls in total. Setup
uses `V34InitializeImplementationSpecific`, `txinit`, `V34SetupModulator`,
`preinitdigital` and `initV34`; the latter supplies the nonzero tables needed
by the mapper's divisions. Half the histories use real scramblers; half use
explicit synthetic callback sources. This is component differential coverage,
not a public modem negotiation reachability claim.

No fixtures, tolerances or mutation snapshots are changed. No modern host
compiler or x86_64 compiler result is part of this recovery.

The full `make phase J=4` gate passes: **394 period differential binaries
passed, zero failed**, followed by the successful phase boundary. For this
run, local `gcc` and `g++` dispatchers route the structural checks through the
same Gentoo container. `CXXFLAGS` is overridden with
`-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-fno-exceptions -fno-rtti -DDSPLIB_REPRODUCE_BUGS` for the parameter/layout
check. The dispatcher log records 603 structural compiler invocations: 455
Gentoo `gcc` and 148 Gentoo `g++`, with no `-m64` argument.

The structural results include 12,967 references with no unresolved or stale
entries, 2,288 matching offset annotations and 4,130 live offset assertions.
The separate upstream checkout is unavailable in the isolated worktree, so
the gate checks its seven files against the manifest only and explicitly
reports that upstream drift was not checked.

## Remaining work

The 86-instruction deficit and different stack frame remain unresolved.
The original accesses configuration through object-relative displacements;
the reconstruction keeps a shell-base pointer. Recovering an enclosing owner
type requires separate layout evidence and must preserve overlapping object
views. No artificial layout or register-allocation constraint is introduced
here to force those accesses.
