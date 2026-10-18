# Bell 103 helper boundaries

Separating four signal-path helpers from the half-duplex callers restores
the reference's calls under the retained Gentoo compiler profile. Two
callers become byte exact: `RxHdxStartB103` (139 bytes) and `TxHdxDataB103`
(54 bytes). There are no exact losses across the 17 shared functions.

## Evidence and ownership limits

The reference's FILE order includes `B103.c`, `B103prc.c`, `B103tab.c` and
`B103int.c`. The local `tx_in_internal` and `rx_out_internal` buffers belong
to `B103prc.c` and anchor the host dispatcher there. They remain in that file.

`ModDataB103`, `TxNoCarrierB103`, `DemodDataB103` and `CarrierDetectB103`
form a consecutive native run at 0x8f9a0 through 0x8fcd0, after the state
functions. The callers contain actual calls to the smaller helpers, whereas
the merged reconstruction inlines them. The existing `B103int.c` FILE slot
is a plausible home for this run, but it has no local symbol that pins these
globals to that filename. The separation is supported by the call graph;
the filename attribution remains an inference.

The baseline's `00.cgraph` explicitly records `CarrierDetectB103` inlined
into both receive callers, `ModDataB103` into the data/marks transmit
callers, and `TxNoCarrierB103` into the silence caller. In the separated
source those definitions are unavailable to this compilation unit. The
first RTL dump retains the calls that the native object contains. No
attributes or compiler-option changes are needed.

## Bounded controls

The replay pins the unchanged source at `a0c079f61` and requires literal
reproduction of both frozen baseline objects. It uses the complete frozen
`.build-config`, Gentoo GCC 3.4.2-r2 and the shared experiment helper's final
`DSPLIB_REPRODUCE_BUGS` define. Commands, source/object hashes, compiler dumps,
all 17 body verdicts and canonical relocations are retained.

Four controls test the merged baseline, separation of carrier detection
alone, separation of all four helpers in existing source order, and
separation in native helper emission order. The carrier-only cell recovers
`RxHdxStartB103`; both four-helper cells also recover `TxHdxDataB103`.
The two helper orders produce the same per-function results. The retained
source follows the witnessed native helper order.

One additional, independently witnessed control changes the receive-data
count local to `unsigned short`. The native caller zero-extends the low word
of `DemodDataB103`'s result at capture and sign-extends it only at the final
short return. A signed local had moved the sign extension to capture time.
The public prototypes and return types remain unchanged.

| Function | Reference | Merged baseline | Separated helpers | Retained unsigned count |
| --- | ---: | ---: | ---: | ---: |
| `RxHdxStartB103` | 139 | 138 | 139, EXACT | 139, EXACT |
| `TxHdxDataB103` | 54 | 106 | 54, EXACT | 54, EXACT |
| `TxHdxMarksB103` | 156 | 203 | 153, SIZE(3) | 153, SIZE(3) |
| `TxHdxSilenceB103` | 130 | 207 | 128, SIZE(2) | 128, SIZE(2) |
| `RxHdxDataB103` | 180 | 177 | 170, SIZE(10) | 171, SIZE(9) |

Numbers are function lengths in bytes; SIZE differences are length gaps.
The larger receive-data gap accompanies recovery of the actual carrier
call and the count's narrowing boundary. Its three call targets
(`DemodDataB103`, `CarrierDetectB103`, `B103NextState`), object fields,
flag updates and consecutive-loss threshold agree with the reference.
Branch layout remains different; the row is still nonexact.

The complete pair retains all 17 function symbols and their strong bindings.
The existing eight exact bodies remain exact. Nontext contents and section
metadata, canonical nontext relocations, exported bindings and resolved
external imports remain unchanged. The four moved helper bodies retain
their reference verdicts. The replay records every changed body, including
changes hidden by an unchanged SIZE grade.

## Validation

The final cleaned source pair literally reproduces the selected experimental
pair. Focused period validation passes four binaries with zero failures:
`t_b103fp`, `t_b103hdx`, `t_b103create` and `t_b103link`. Existing receive
tests report 41,079 half-duplex checks, 120 direct `RxHdxStartB103` checks
and 62,820 demodulator checks; the bit-error-rate fixture remains green in
both directions. No test input or tolerance changed.

Searching the mutation registry by both source paths and case-insensitive
Bell 103 test names finds zero registered suites. There are no mutation
anchors to relocate with these four bodies.

```sh
python3 tools/recovery_b103_helper_boundary.py \
  --objects build/copy293-baseline --out /tmp/b103-helper-boundary
make period T='t_b103fp t_b103hdx t_b103create t_b103link' J=8
```

The replay does not run differential tests. Full `make phase` and strict
partial-link checks remain integration gates; the function gains do not
establish whole-object identity.

## Post-separation count and control recovery

The next bounded experiment starts from the integrated separation at
`eaf95d618`. It recovers three further byte-exact bodies without changing
any helper, prototype or compiler option:

| Cell | Marks (ref 156) | Silence (ref 130) | Receive data (ref 180) |
| --- | ---: | ---: | ---: |
| Baseline | 153 | 128 | 171 |
| Marks unsigned count | 154 | 128 | 171 |
| Silence unsigned count | 153 | 130 EXACT | 171 |
| Both unsigned counts | 154 | 130 EXACT | 171 |
| Marks zero-first tone-plan branch | 156 EXACT | 130 EXACT | 171 |
| Receive shared result | 156 EXACT | 130 EXACT | 174 |
| Receive carrier-loss-first branch | 156 EXACT | 130 EXACT | 180 EXACT |

The four initial cells cross the two count carriers independently. Native
Marks captures AX with `movzwl` at 0x8f557, then returns through
`movswl %di,%eax` at 0x8f572. Silence captures AX unsigned at 0x8f5de
and sign-extends SI at both returns (0x8f5ef and 0x8f60f), including the
state-transition arm. Unsigned locals retain those boundaries; the public
return type stays `short`. The first RTL dump already distinguishes the
unsigned SI carrier and final HI-to-SI sign extension from the baseline's
sign-extended SI carrier and plain SI return copy. This is a type witness, rather than a register preference.

Marks tests the tone-plan selector at 0x8f562 and jumps on nonzero at
0x8f568. Its zero arm checks both the expired transmit counter and receive
state greater than 14; its nonzero arm needs only the expired counter.
Writing these arms in native orientation restores the shared state-change
call tail. The branch topology is present in the first RTL dump, before
register allocation and final block placement.

Receive data captures the demodulator count unsigned in EDI (0x8f66b).
Carrier detection's nonzero result branches to the carrier-present arm
(0x8f67c): it sets the carrier flag and resets the loss counter. The absent
arm increments the signed short counter; values through seven retain the
count. Above seven it calls the selected next-state function, clears flag
0x80, zeros EDI, returns via `movswl %di,%eax`, and publishes status 6.
The other two paths also clear flag 0x80 and return through the same signed
low-word conversion. The prior source returned a literal zero on timeout;
the native zeroing of the saved count carrier independently supports the
shared-result source. The intermediate shared-result cell remains nonexact;
the separately witnessed branch orientation completes recovery. These CFG
changes are visible in the first RTL dump. The compiler still duplicates
machine return tails; the source requires neither a forced join nor padding.

The final pair has 13 exact bodies out of 17, up from 10. Exactly these
three bodies change; the other 14 bodies and their canonical relocations
are unchanged. All 17 strong function exports, nontext bytes and section
metadata, nontext relocations and resolved external imports are unchanged.
EXACT here checks instruction bytes and relocation targets/addends, rather
than mnemonic similarity.

Focused period validation again passes four binaries with zero failures:
`t_b103fp`, `t_b103hdx`, `t_b103create` and `t_b103link`. The existing
component histories include transmit countdown/state changes and tone then
silence through receive carrier detection/loss. The half-duplex fixture
reports 30,600 transmit and 41,079 receive checks, with receive-data visited
215 times. These are component fixtures, including shortened timeout setup;
they do not establish public modem end-to-end reachability for every planted
state. The unchanged link fixture also passes. No test input, amplitude or
tolerance changed, and there are still no registered Bell 103 mutation
suites to reanchor.

The replay requires raw reproduction of the two frozen post-separation
objects before interpreting any cell. It records complete commands,
source/object hashes, all 17 body verdicts and relocations, and every GCC
RTL pass. Its final generated source must equal the adopted source.

```sh
python3 tools/recovery_b103_count_control.py \
  --objects build/b103-eaf-baseline --out /tmp/b103-count-control
make period T='t_b103fp t_b103hdx t_b103create t_b103link' J=8
```

The historical separation table above records its own baseline; its three
residual rows are superseded by this follow-up. Full `make phase` remains
an integration gate.

## Receiver owner reloads after calls

A further bounded cross starts at `64d53dd51`, after the three count/control
recoveries. `RxDetMarkB103` cached both `fp->hdx` and `fp->dsp` on entry.
The reference instead obtains each owner when used across the two external
signal-processing calls: it reloads `fp->hdx` after AGC at 0x8f374, then
reloads `fp->dsp` and `fp->hdx` after tone detection at 0x8f387/0x8f38a.
The timeout arm independently reloads `fp->hdx` at 0x8f3c0. These are actual
operand value-age differences, even if the tested callees leave the owners
unchanged. No hypothetical callback fixture is needed to justify the source
boundary.

| Owner access | Receiver length | Reference verdict |
| --- | ---: | --- |
| Both cached, baseline | 246 | SIZE(8) |
| HDX direct, DSP cached | 247 | SIZE(9) |
| HDX cached, DSP direct | 241 | SIZE(3) |
| Both direct, retained | 238 | BYTES(6) |

The native receiver is 238 bytes. The retained source accesses owners directly
rather than keeping either pointer alive across calls. The first RTL dump
contains the post-call owner loads; by register allocation the unnecessary
long-lived owners and extra saved register disappear. The final stack frame,
save set, call targets and branch layout agree with the reference. The six
remaining differing instruction bytes select scratch registers in the
next-state index and timeout comparison; no missing field, operation or
branch is established there. No register spelling experiment is adopted.

Only this receiver body changes. The other 16 bodies and their canonical
relocations, all 17 strong function exports, nontext bytes and section
metadata, nontext relocations and resolved external imports are unchanged.
The pair retains 13 exact bodies out of 17, with zero exact gains or losses.
This is a supported partial source recovery, rather than a new exact claim.
The four focused period binaries again pass with zero failures, using the
unchanged component fixtures described above.

`B103AnswerNextState` was reviewed alongside this receiver. Its native
fields, widths, case predicates and diagnostic guards agree with source.
Its prior closed MRF interaction (`tools/batch20_b103_mrf_stages.py`) reports
identical RTL through `27.flow2`, with the first difference at `28.peephole2`
in scratch-register choices. That history provides no new source witness;
the 250-byte candidate versus 237-byte reference remains nonexact.

```sh
python3 tools/recovery_b103_receiver_owners.py \
  --objects build/b103-count-baseline --out /tmp/b103-receiver-owners
make period T='t_b103fp t_b103hdx t_b103create t_b103link' J=8
```

The replay raw-reproduces both frozen baseline objects, crosses both owner
choices independently, and retains the same complete pair controls and
compiler dumps as the preceding replay. It also checks that the retained
cell's source literally equals the adopted source.

## Modem loop and circular-index widths

The next screen starts from `76630dbd7`. Constructor diagnostics and shared
schema history were reviewed first; no new missing constructor call was
established. The modem dispatcher supplies independent width witnesses.
Its five outer loops sign-extend the incremented low word at 0x8f042,
0x8f074, 0x8f165, 0x8f1a0 and 0x8f2a2. Source instead used an `int` index.
The first copy, mark-fill and input-filter loops compare against zero-extended
count words, whereas source explicitly cast those limits to signed short.
The pad loop still begins at the signed input count; the output-copy limit
was already unsigned. Only the three mismatched limit conversions change.

| Cell | Modem bytes | Reference verdict | Changed bodies |
| --- | ---: | --- | --- |
| Baseline | 714 | SIZE(61) | none |
| Short outer index | 734 | SIZE(41) | modem, constructor |
| Unsigned outer limits | 714 | SIZE(61) | modem |
| Both outer widths | 751 | SIZE(24) | modem |
| Direct signed-short circular field, retained | 783 | SIZE(8) | modem |
| Later filter owner captures, rejected | 799 | SIZE(24) | modem |

The reference modem is 775 bytes. The independent outer cross distinguishes
index narrowing from limit extension, including the otherwise unchanged
length of the unsigned-limit cell. First RTL already contains the short
index's narrow/sign-extension boundary and the unsigned count conversions.
The short-index-only cell also perturbs the constructor's register choices;
the retained combined source leaves that bystander entirely unchanged.

The filter has a separate field-publication witness. Native code increments
the low word of `bpf_idx`, compares that signed word to `bpf_taps` at
0x8f0ac, and publishes either the incremented word or zero. It loads the
signed index for history access and reloads it after the history store
(0x8f0f0). The previous widened local compared an untruncated increment
and retained the local across the history write. The retained source
increments/resets the actual short field and reads it at each use. Its
first RTL preserves the HI comparison and post-store field read. This is
native arithmetic and value-age evidence, without a claim that invalid
internal indices arise in normal operation.

One further cell moves history, coefficient and tap captures to their
observed native use boundaries. GCC still hoists the pointer loads outside
the filter loop, so that cell does not recover the native load graph. It
is recorded and excluded. The remaining modem mismatch includes that
unresolved load/alias context and allocation/scheduling differences; the
source is still nonexact. No additional declaration or register palette was
run, and no original source spelling is claimed uniquely recovered.

The retained pair changes only `B103FP_modem`. Other 16 canonical bodies and
relocations are unchanged; all 17 strong exports, nontext bytes and section
metadata, nontext relocations and resolved imports agree with baseline.
All 13 exact bodies remain exact. The unchanged four focused period
fixtures pass with zero failures, including the existing 120,048 modem
checks and real link fixture. Those fixtures exercise normal component
counts and circular-filter wraps; they do not establish reachability for
negative counts, oversized staging requests or corrupted internal indices.
No such synthetic fixture was added to justify the correction.

```sh
python3 tools/recovery_b103_modem_widths.py \
  --objects build/b103-owner-baseline --out /tmp/b103-modem-widths
make period T='t_b103fp t_b103hdx t_b103create t_b103link' J=8
```

The six-cell replay pins the source revision, raw-reproduces both frozen
post-owner objects, preserves complete commands and RTL dumps, checks the
full pair for each cell, and requires retained source equality. The rejected
owner-capture cell stays visible. Full `make phase` remains the integration
gate; the eight-byte length gap alone is not an instruction-distance claim.
