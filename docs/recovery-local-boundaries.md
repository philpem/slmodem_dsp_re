# Resolved local-call boundary screen

This finite pass yields **zero whole-function byte-exact gains and no source
change**. It checks the 50 retained direct calls newly resolved by exact ELF
function ownership, including callers outside the earlier source/size filter.
The one independently supported GetGain lifetime control reproduces the
observed private rounded-result carrier but does not reproduce the function.
The source candidate is declined.

## Inputs and denominator

The source and period-object baseline is `14f852aa`. The input residual ledger
was generated at `b292359f831c77f536465a58d88deaacc726a852`; its SHA-256 is
`f95576d5a4499c1e3f8b863a52a348792f777f08a568773411a752ca5eba85e2`.
Every selected defining object is checked against that ledger's object hash.
All six selected objects agree despite the different source revisions. The
reference hash is checked too. No local target is inferred from a span name or
disassembler annotation. The input ledger's **95 indirect calls remain
unresolved**; this screen does not assign them a callee or prototype.

`tools/recovery_local_boundary_screen.py` archives both sides of each resolved
local call, with sixteen instructions before and after it. These windows aid
manual review and are not a def/use analysis. Positive/negative controls cover
equal, reordered and truncated target sequences and a register-extension
nomination. Every archived call address must identify exactly one decoded
CALL; an object or reference hash mismatch stops the screen.

The measured denominator is **6 objects, 17 retained callers, 50 retained local
sites**. Those callers contain 27 original local sites. Fourteen callers have
equal ordered local-target sequences, giving 19 tentative pairs. The other
three callers account for 31 retained sites and are archived without pairing:

| Caller | Retained local calls | Original local calls | Disposition |
| --- | ---: | ---: | --- |
| DialerProgress | 2 | 3 | Different topology; no ordinal pairing. |
| probeselect | 10 | 0 | Retained outline helper absent as a call in the original. |
| v34handshak | 19 | 5 | Different helper topology; no ordinal pairing. |

Of all 50 retained sites, 38 are in source families the older filter excluded,
three are in callers above its 850-original-byte limit, and nine were eligible.
Resolving the callee identity widens coverage; it does not establish its return
type or make every pair a corresponding execution point.

## Review and closed domains

| Target family and paired sites | Evidence and disposition |
| --- | --- |
| AnalyseDialString: 2 sites in DialerCreate and IsDialStringInvalid | Full EAX verdicts, with existing integer source contract. The unsigned promoted-verdict/capture control is already closed in `batch100-dialer-verdict-domain.md`: its 149-byte candidate differs only in the raw local CALL displacement. Constructor boundaries are covered by `batch20-dialer-create-boundaries.md`. No reopening. |
| bValidateEnergyValue / EchoCanceler: 4 sites in FDSP_Kernel_Loop | Full integer validation results and object/buffer pointers; no new narrow-result contradiction. `energy-callback-result-domain.md`, its results and `batch50-fdsp-echo-boundaries.md` already cover success, RMS/index capture, quiet predicate/history ownership and count controls. The stale comment describing bValidateEnergyValue as external does not describe its current static definition. |
| GetGain: 4 sites in beepgen_create, beepgen_start_beep, beepgen_start_dtmf and beepgen_sample | All original and retained sites pass the same object and distinct field addresses. The constructor passes gain1=`bg+0x14`, gain2=`bg+0x18`; the other three pass the reverse pair. The existing constructor anomaly is preserved. The callee's private-result lifetime is a separate witness, tested below. |
| V34demodulate: 4 sites in receiver / rxtiming | The formal argument is the full receiver pointer; no call-result width exists. One extension nomination is a false physical-order pairing, described below. |
| getbit: 1 recursive site | Integer bit result with no extra retained argument extension or independently contradicted width. No new source preimage established. |
| getMPrecvdBits: 4 sites in the V.90/V.92 phase and renegotiation callers | Object pointer argument; void source result and no caller-consumed result. Exact local ownership adds no narrower return witness. |

The single extension nomination is `receiver`'s first physical V34demodulate
call. Original `0x5bef0` is the optional second-wrap pull and `0x5bf13` is the
common pull. In the retained body the common call is first at `0x1857`, while
the optional call is in a later block at `0x1fce`, followed by a jump to the
common preparation at `0x1839`. Thus equal repeated-target sequences still
pair different control-flow paths. The extra nearby extension does not supply
a different formal argument. The source at `V34RX.c:1335` already represents
the optional pull followed by common previous-sample publication and pull.

The unmatched handshake/probe topology is not evidence of missing semantic
code. F11512 and F11519–F11523 in `findings.md` record the force-inline,
shared-tail, call-boundary, cross-jump and rejoin experiments. This screen adds
no independent discriminator that reopens them. It also does not reopen
B.103, the FDSP controls or the Dialer controls listed above.

## GetGain private float-result experiment

The complete original GetGain is `0xac960..0xacb10` (432 bytes). At
`0xaca0d` it stores the first pow product as a private **float** at `esp+0x34`,
reloads that float at `0xaca11`, publishes it through gain2 at `0xaca15`, then
calls the second pow at `0xaca33`. The subsequent `FMULS esp+0x34` at
`0xaca38` uses the private rounded value. The retained body instead stores
through gain2 at `0xb1` and uses `FMULS (gain2)` at `0xd5` after pow. Both
debug print arms subsequently reload their output pointers in the original.

This is a read-age and value-ownership witness. It alone does **not** establish
a behavioural defect: the known callers supply distinct ordinary float
fields, and pow receives scalar doubles rather than either output pointer.
No fixture with an invented pow side effect or unproven alias is introduced.
The known constructor, start and sample lifecycle cases remain the valid
component-method boundary.

The predeclared two cells are unchanged full TU and one candidate:

```c
float level = (float)(pow(10.0, (double)atten * -0.05 + base) * 0.276);
*gain2 = level;
*gain1 = (float)(pow(10.0, (double)-diff * 0.05) * level);
```

The prediction is that a private float local restores the rounded result's
ownership and might explain the complete 432-byte body. A persisting earlier
arithmetic difference, wrong size or differing operands falsifies that full
explanation. Literal widths, debug reloads, parameter query order, prototypes,
source order and compiler options stay fixed. The existing allocation
common-return experiment in `batch100-beepgen-return-domain.md` and the
sin/cos profile study in `gcc3-alignment-callee-inventory.md` are separate,
closed domains. The historical F11358 text "GetGain 432/432 bytes exact"
records equal size after static binding; the present strict body verdict is
BYTES, so that prose is not evidence that this lifetime domain was tested.

`tools/recovery_local_gain.py` uses the shared full-TU replay driver and
`experiment_toolchain.compile_shell`, which appends DSPLIB_REPRODUCE_BUGS
after configurable flags. The baseline `.build-config`, full commands and
source/header hashes are saved in `build/recovery-local-gain/results.json`.
The actual driver is Gentoo GCC 3.4.2-r2 and its selected assembler is GNU
2.15.92.0.2 (20040927). The flags are the baseline O3/register-renaming/i386/
i686/x87/no-IEEE/omit-frame-pointer/accumulate-outgoing-args profile, with
the same headers and forced period compatibility header. `-da -dP` request
diagnostic dumps in each cell's own working directory. The unchanged cell
raw-reproduces the archived production object before the candidate is scored.

| Cell | GetGain strict verdict | Exact functions in complete TU | Changed bodies |
| --- | --- | ---: | --- |
| Unchanged | 432 bytes, BYTES 219 | 9/23 | Control |
| Private float result | 428 bytes, SIZE 4 | 9/23 | GetGain only |

The candidate recovers the private FSTPS/FLDS/published FSTPS and post-pow
FMULS carrier. It leaves an earlier independent difference: original
`0xac9b0..0xac9c5` computes the base as FILD, FMULL constant 0.05, FSTPL.
The candidate computes FILD, FSTPL, FLDL constant 0.05, FMULL spilled integer,
FSTPL. The first pow exponent also retains a different x87 evaluation graph.
The frame stays 0x4c against the original 0x5c. A private-result local therefore
does not explain the complete body. No arithmetic spelling or profile sweep
is justified by this experiment.

There are **zero exact gains and zero losses** among all 23 reference-shared
emitted functions. Every other emitted function is unchanged. The `.text`
section remains 4,730 bytes because alignment absorbs the four-byte reduction.
Allocated nontext data and nontext relocations are byte identical; all symbol
names, bindings, visibility, offsets and exports are unchanged. The only
symbol-record change is GetGain's size, 432 to 428. Baseline object SHA-256:
`f1c2f5ff328ef961558ddbb4ee7a7ea3815268d42c88652687ff582dc251d64a`.
Candidate SHA-256:
`6e715f08495248e3420354451da2ae05400fe3d8a1799ff1596b936c95d69bcc`.

The candidate passes `make period T=t_beepgen J=4`: one requested binary,
one passed, zero failed. Its fifteen groups contain 178,693 checks, including
243 direct gain checks,
14 constructor checks, 2,900 start-beep checks, 537 DTMF checks and 49,064
whole-queue sample checks. The source was restored after this measurement.
The complete candidate log is preserved as
`build/recovery-local/gain-candidate-fixture.log`; this functional pass does
not override the byte-identity miss. The source candidate is not retained.

## Reproduction and artefacts

Run from the worktree with a ledger and objects whose recorded hashes agree:

```sh
python3 tools/recovery_local_boundary_screen.py \
  --ledger /path/to/residual-ledger.json --objects build/tc_repro \
  --reference /path/to/dsplibs.o \
  --output build/recovery-local/local-boundaries.json
python3 tools/recovery_local_gain.py --baseline-dir build/tc_repro \
  --domain 'Retained full TU versus private float first-pow result across second pow; no other source or option axis'
```

The first tool records the 50-site denominator and all five apparatus controls;
the second refuses an unchanged-object mismatch. Reports are
`build/recovery-local/local-boundaries.json`, `local-screen.log`,
`gain-whole-tu-audit.json`, `gain-candidate-period.log` and
`gain-candidate-fixture.log`; the two-cell complete objects, RTL streams,
commands and verdicts are under `build/recovery-local-gain/`.

After restoring production source, `make phase J=4` exits zero with **388
period differential binaries passed, zero failed**, 335 types and zero
duplicate definitions, and 285 mutation suites / 10,040 static anchors with
zero invalid anchors. Its log is `build/recovery-local/final-phase.log`.
Both new Python tools pass `py_compile`; the screen reports all five apparatus
controls. The 300 canonical objects still agree with the frozen input ledger
apart from the already-recorded FixedRC recovery; Beepgen retains the baseline
SHA-256 above. No source, header or fixture change is part of this pass.

The next source experiment requires a new independent caller/callee width,
reachable value-age, or earlier compiler-stage witness. This bounded negative
does not establish a global ceiling or attribute every residual to register
allocation.
