# JTXMIT owner and continuation recovery

This bounded recovery starts from master `cc6377b2c6352923e4ad9f092cb905d0d3c4738a`.
It moves the complete JTXMIT/J1TXMIT transmit arm into `v34handshak`, where the
reference places it, and uses that function's saved transmitter and receiver
owners. It also restores the J1TXMIT diagnostic before the scrambler clear.
The remaining handshake arms and whole-function exactness remain open.

All compilation in this investigation used
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, Gentoo GCC 3.4.2-r2,
with the retained period flags. No compiler-option or attribute change is
part of this recovery.

## Native witnesses

The reference transmit table sends states 64 and 68 to `0x635cc`. The countdown
completion at `0x6533c` rejoins the common body at `0x635f0`. The complete arm
includes the role-0x66 continuation at `0x66ca8`, the state-68 completion at
`0x65653`, and the state-64 transition and forced echo freeze at
`0x6372e..0x63853`. Each returns to the enclosing transmit loop.

The native owners are visible independently of the source hypothesis:

| Native operation | Reconstructed access | Object offset and width |
| --- | --- | --- |
| `0x635ff`: load `0x3bc` through saved `obj+0x221c` | `frame.txb->unmapped_3ba + 2` | `0x25d8`, 16 bits |
| `0x6360d`: load `0x3ba` through the same owner | `frame.txb->unmapped_3ba + 0` | `0x25d6`, 16 bits |
| `0x63711`: compare `0x3be` through the same owner | `frame.txb->unmapped_3ba + 4` | `0x25da`, 16 bits |
| `0x63697..0x636b9`: quadrant and point publication | `frame.txb` members | `0x25c6/0x25c8`, 16 bits; `0x25d0`, 32 bits |
| `0x6361e`: form scrambler address from object | `&obj->tx.tx_scr_sr` | `0x25cc`, 32 bits |
| `0x66ca8`: load saved receiver, then test flags | `frame.rx->flags` | receiver `+0x122`, 16-bit field |

The existing layout assertions establish `v34_tx` at object `+0x221c` and its
`unmapped_3ba` region at transmitter `+0x3ba`. The conversions preserve the
old raw accesses' owner, resolved byte offset and width; they do not assign
new semantic names to the unmodelled region.

At `0x65653` the reference tests the diagnostic level. Its debug path calls
`dsplibs_debug_printf` at `0x6915b` and returns to `0x65660`. Only then does
`0x65676` clear the scrambler, followed by the state, previous quadrant and
segment-count stores. The former helper cleared the scrambler before the
state setter printed. The retained arm spells this known-state diagnostic
and its following stores in native order. This is an observed publication
boundary; no production failure from the old ordering was demonstrated.

## Bounded controls

Counts below exclude alignment padding. LCS uses the larger stream, which
is the reference's 12,199 instructions in every cell; its register
canonicalisation is a structural measure, not a proof of equivalence.

| Complete-TU control | Root instructions | Longest run | LCS | `cwtl` |
| --- | ---: | ---: | ---: | ---: |
| Master baseline | 3060 | 27 | 1647 | 22 |
| Complete arm in its enclosing switch | 3455 | 27 | 1848 | 27 |
| Also recover saved transmitter/receiver owners | 3316 | 30 | 1811 | 27 |
| Also recover J1TXMIT print/store chronology | 3315 | 30 | 1811 | 27 |
| Direct two-bit scrambler loop, rejected | 3315 | 30 | 1810 | 27 |
| Native promoted-int right shift, retained | 3315 | 30 | 1812 | 27 |

The owner control follows the native bases despite a smaller LCS than the
plain copy. The direct-loop control has no demonstrated benefit and was
reverted. Independently, `0x6360d` zero-extends the source halfword and
`0x6362f` uses arithmetic right shift. Removing the forced unsigned-int
conversion restores the normal integer promotion and this native operation.
The zero-extended input is non-negative, so its values are unchanged.
The general scrambler already expands in this arm. Recovering the
enclosing owner admits its two polynomial loops into `v34handshak`; changing
the scrambler algorithm is unnecessary here.

Among 52 functions common to baseline and retained TU, only `v34handshak`
and `datapumpv34` change canonical bodies; the other 50 remain identical.
The removed local `v34tx1_jtxmit` body is now inside the dispatcher. There
are 29 native-named common functions and all nine baseline EXACT functions
remain EXACT. The collateral `datapumpv34` change is 1035 to 1012 bytes;
its longest matching run remains 50, but LCS falls from 194 to 186 of 211.
The observed differences reorder independent stores and coalesce a zero-store
tail. The fields, constants and `v34handshakinit` call boundaries remain;
this collateral change is reported as a structural regression, not a gain.

The initial collateral screen overcounted five identical bodies because it
interpreted an unresolved anonymous-relocation verdict as a change. Direct
body/relocation tuple comparison and a second comparison masking relocation
bytes both restrict the actual changes to the same two functions.

## Differential boundary and controls

`t_v34hstb1` runs the complete reconstructed handshake against the reference,
including all 25 supported transmit-state entries and multi-pass budgets.
The focused retained run passes 5901 checks.

`t_v34hstx1`'s 26 JTXMIT/J1TXMIT cases now call the complete reconstructed
`v34handshak`. The obsolete standalone helper and its duplicate body are
removed. The final focused run passes 25,836 main-suite checks, plus the
existing 53/46 table checks. These planted component states exercise both
polynomials, wraps, both state completions, countdown completion, forced
echo freeze and the role-0x66 precedence. They remain **synthetic component
fidelity probes**; this change does not establish a public modem handshake
history. An intermediate fixture ran both bodies (28,228 checks); removing
its duplicate helper checks accounts for the final smaller denominator.

A single named sensitivity control changed only the root's transmitted point
to `point ^ 0x10000`. On the final fixture it failed 104 of 25,888 main-suite
checks, first reporting object differences at `+0x218e` and `+0x25d2` in case
6400. This confirms that the fixture observes the compiled enclosing arm.
The source was then restored. This was a bounded negative control, not a
mutation campaign. Removing the obsolete helper definition leaves the
canonical TU's code and all metrics above unchanged.

All 300 canonical objects were compared against the fresh master build.
The other 299 objects are byte-identical; the sole changed TU preserves its
nine exact native functions and gains none. The census remains 1143 EXACT,
35 REGALLOC, 84 BYTES and 590 SIZE across 1852 symbols, with zero unresolved
relocations. Full Gentoo phase validation passes: **394 differential suites,
zero failures**, and all structural checks. The structural compiler calls
were also routed to Gentoo using the scratch compiler wrapper; no host
compiler was invoked.

## Partial-link limits

Both 300-object links use the same recovered input order and Gentoo `ld -r`.
The changed TU removes one outlined helper and reduces the partial `.text`
size by 80 bytes. The reference comparison remains DIFFERENT.

| Positioned partial-link measure | Master | Candidate | Denominator |
| --- | ---: | ---: | ---: |
| Equal allocated-content bytes | 72280 | 72560 | 943398 |
| Exact relocation records | 999 | 995 | 18317 |
| Exact symbol records | 457 | 457 | 2931 |

The net relocation loss is eight lost and four gained positioned records.
These records include position coincidences between different functions:
for example, reference `0x74081` is `V8Create+0x261`, whereas baseline has a
matching debug-level relocation at `GetDialerConfig+0x41`. The latter moves
by 80 bytes with its unchanged source object. At `0x979bf`, the reference
owner is `V17RX_create` and the baseline owner is `RxHdxIdleV17`; that source
object is also byte-identical. Other changed records lie in the reorganised
handshake region. The full list and owner attribution are retained in
`scratch/jtx-partial-exact-delta.json` and `scratch/jtx-partial-owner-delta.json`.

These observations explain part of the positioned census; they do not relax
it. The four-record net decline remains visible. The current partial-link
CI gate ratchets binding mismatches, not positional counts; no gate or
acceptance threshold is changed. Function-level preservation and
an improved root LCS do not establish a whole-object identity improvement.

## Relationship to the wider handshake draft

[Draft PR303](https://github.com/philpem/slmodem_dsp_re/pull/303), inspected at
`40a22b1bd6392b6931d459ef25406bb872f75e51`, moves many handshake arms and removes
implementation headers. Its review explicitly records no whole-object identity
gain. This change is a separate, single-arm recovery against current master;
it does not adopt that branch or establish that the wider rewrite improves
identity. The existing component fixture now exercises the recovered arm
through its enclosing dispatcher; there is one source body.

## Separate unresolved inline boundaries

Fresh, unchanged builds with `-da`, `-save-temps`, `-fdump-tree-original` and
`-fdump-tree-inlined` reproduce all 105 function bodies in the three relevant
TUs (53 V90 modulator, 44 V92 modulator, eight encoder). Diagnostics do not
change their canonical bodies. Reading `lang_independent_params` from the
installed Gentoo `cc1plus` confirms `max-inline-insns-auto=100` and
`max-inline-insns-single=500`.

| Callee | Frontend cost | Current caller | Reference caller |
| --- | ---: | --- | --- |
| `V90Phase4Modulator::recivedSUVtag` | 82 | inlined, 47 instructions; SIZE 159 | one sibling jump |
| `V92Phase4Modulator::recivedSUVtag` | 100 | one sibling jump, EXACT | one sibling jump |
| `V92ConvolutionEncoder::inverseMap` | 98 | inlined, 150 instructions; SIZE 218 | retains helper call |

All three out-of-line callees are themselves EXACT. The `.00.cgraph` files
explicitly record the V90 and encoder inlining decisions. The naturally
retained V92 call is the boundary control and must be removed from any list
claiming both wrappers remain blocked.

These observations distinguish frontend source cost from final machine-code
size. They do not recover the original V90/encoder source representation or
prove that a legal source route is impossible. Further useful evidence would
identify the original callee's pre-optimisation expression structure or
visibility and preserve its exact out-of-line body while retaining the native
call. Adding operations solely to cross the threshold, moving a function
without TU evidence, or changing an inline option would not supply that
missing evidence. No V90/V92 source change is retained.

## Further routes: DIL and the progress dispatcher

The same method found a separate DIL control: native `V34XF_IndicateDilReceived`
forms engine `obj+4` at `0xa30f` and transmitter `obj+0x221c` at `0xa316`,
both before the diagnostic. Its stores at `0xa323` and `0xa330` address
engine `+0x248` (object `+0x24c`, 32 bits) and transmitter `+0x3a4`
(object `+0x25c0`, 16 bits). Giving those existing owners to the complete TU
raises LCS 13→27/36 and longest run 6→12, preserving all 30 exact neighbors.
That independently evidenced change is being delivered on a separate branch;
this JTX change does not include it. The other raw changed body,
`k56FlexPhase34`, changes only a local call displacement to the unchanged
`getMPrecvdBits` target after DIL's size moves its caller; its instruction
sequence and 13 relocation records are unchanged.

A fresh read of `VPcmV34Progress` changes the next question. The native
function at `0xb3c0` has five direct calls to `VPcmV34InitiateRetrain`; the
current canonical object has eight. The complete symbol extents were checked
for calls and tail jumps: all five/eight relocations follow opcode `e8`;
there are no tail jumps to this callee in either function. All nine source requests have native retained
call paths, with shared tails explaining the smaller native call count:

| Native incoming blocks | Requested DP | Shared call |
| --- | --- | --- |
| `0xcbcf`, `0xcbec` | `0x22`, `0x5a` | `0xcbe2` |
| `0xcdc7`, `0xcde4` | `0x22`, `0x38` | `0xcdda` |
| `0xcdf9`, `0xce16`, `0xce2b` | `0x22`, `0x5a`, `0x5c` | `0xce0c` |
| tone-detector continuation | zero | `0xb688` |
| waiting-user timeout | zero | `0xb97c` |

Consequently, a retrain call-count difference is not evidence of missing
inlining. A bounded next control would let each of the three inner
switches select its request before a common call, then inspect the first RTL
stage that merges those tails. The falsifier is unchanged or worse tail
sharing, a changed behavior at the component boundary, or any lost exact
neighbor. Shared machine tails alone do not prove which source form the
author used; no such Progress source change has been tested or adopted here.
Progress already declares engine, transmitter and receiver owners, so the
successful DIL owner spelling is not a missing change to copy blindly.
The native and current complete disassemblies are retained as
`scratch/progress-native.txt` and `scratch/progress-candidate.txt`; the
complete call/tail inventory is `scratch/progress-retrain-edges.json`.

## Reproduction and evidence

Use `tools/dis.py ref/slmodemd/dsplibs.o START STOP` for the native ranges
above. Build the full TU with `make tc`; compare `v34handshak` with
`tools/toolchain/structsim.py`, and audit native-named neighbors with
`tools/toolchain/byteident.py`. Run the focused oracles with
`make period T=t_v34hstb1 J=2` and `make period T=t_v34hstx1 J=2`.

The isolated investigation tree is `/tmp/slmodem-astra-unresolved`.
`build/audit/` preserves each source/object control and its measurements;
`scratch/inline-dumps/` contains the unchanged diagnostic objects, callgraphs,
parameter proof and body-comparison results. `scratch/jtx-single-home-negative.log`
and its `.run.log` preserve the final firing control. These local artefacts supplement
the native-address and result record above; they are not build prerequisites.
