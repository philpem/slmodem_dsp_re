# What `dsplibs.o` actually is

Every claim here is reproducible with the command shown. Run from `slmodemd/`
unless stated otherwise. Nothing in this document derives from any source other
than `dsplibs.o` itself and the `slmodemd/*.c|*.h` sources.

---

## 1. It is an `ld -r` partial link that kept its filenames

283 `STT_FILE` entries survive in the symbol table, one per original
translation unit, in link order. Two are GCC synthetic (`<built-in>`,
`<command line>`), leaving **281 real translation units**. `voice.c` appears
twice.

```sh
readelf -sW dsplibs.o | awk '$4=="FILE"{print $8}' | sort -u
readelf -p .comment dsplibs.o | head -1     # GCC: (GNU) 3.4.2
```

This is the single most important property of the object: the original module
structure is recoverable, so the reconstruction mirrors it rather than
inventing a layout.

**Language mix: 209 `.c`, 70 `.cpp`, 1 `pow.S`** (`tools/tumap.py`).

## 2. The C++ is "C with classes"

134 `.gnu.linkonce.t.*` template-instantiation sections and 4 vtables, but
**zero** `new`/`delete`, `__cxa_*`, `_Unwind_*` or `_ZTI*` references — built
`-fno-exceptions -fno-rtti` with no dynamic allocation.

```sh
nm dsplibs.o | grep -cE '_Znwj|_Znaj|_ZdlPv'      # 0
nm dsplibs.o | grep -cE '__cxa|_Unwind|_ZTI'      # 0
readelf -SW dsplibs.o | grep -c gnu.linkonce      # 134
```

Recoverable template instantiations, from the mangled names:
`GenericIIR<float,double>`, `SineWave<float,float>`, `Queue<float>`,
`Scrambler<int,unsigned char>` / `<unsigned char,int>` /
`<unsigned char,unsigned char>`, and the `Resampler` →
`ResamplerTiming` → `ResamplerTimingOffset` / `V90Resampler` virtual hierarchy.

## 3. The public ABI is 22 symbols out, 24 in

Everything else — 2600+ symbols — is private, reached only through
`struct dp_operations` function pointers. That narrow waist is what makes
module-by-module replacement viable.

**Out** (declared `extern` in `modem.c:73-100`, `modem_main.c:102-103`):
`prop_dp_init/exit`, `dp_runtime_create/delete`, `dcr_create/delete/process`,
`RD_create/delete/process/ring_details`, `CID_create/delete/process`,
`VOICE_create/delete/process/command`, `FAX_create/delete/process/class1_command`.

**In** (`nm -u dsplibs.o`): 9 × `sysdep_*`, `modem_dp_register/deregister`,
`modem_get_bits/put_bits`, `modem_get_param/set_param`, `modem_get_sreg`,
`modem_send_to_tty/recv_from_tty`, `dsplibs_debug_level/printf`,
`modem_debug_log_data`, `memcpy`, `__divdi3`, `__moddi3`.

## 4. Seven init functions register 13 `DP_ID`s

```sh
objdump -d -r -j .text --start-address=0 --stop-address=0x60 dsplibs.o
```

`prop_dp_init` calls, in order: `dp_call_init`, `dp_b103_init`, `dp_v22_init`,
`dp_v23_init`, `dp_v32_init`, `dp_v8_init`, `dp_vpcm_init`.

| init | registers |
|---|---|
| `dp_call` | 2 `DP_CALLPROG` |
| `dp_b103` | 103 `DP_B103`, 21 `DP_V21` |
| `dp_v22` | 122 `DP_V22BIS`, 22 `DP_V22`, 212 `DP_B212` |
| `dp_v23` | 23 `DP_V23` |
| `dp_v32` | 32 `DP_V32`, 132 `DP_V32BIS` |
| `dp_v8` | 8 `DP_V8` |
| `dp_vpcm` | 34 `DP_V34`, 90 `DP_V90`, 92 `DP_V92` |

`DP_AUTOMODE`, `DP_V17`, `DP_FAX`, `DP_K56`, `DP_V8BIS` are **never
registered** despite the code existing — those paths are reached internally.
The reconstruction must not invent registrations for them.

## 5. The datapumps are already native 8 kHz

`b103/v22/v23/v32_create` all pass `0x1f40` (8000) to `dp_wrapper_create`;
`v8_create` and `vpcm_create` call it not at all and run at the host rate.

```sh
for f in b103_create v22_create v23_create v32_create v8_create vpcm_create; do
  read a n <<< $(nm -S dsplibs.o | awk -v s=$f '$4==s{print $1,$2}')
  echo -n "$f: "
  objdump -d -r -j .text --start-address=0x$a --stop-address=$((0x$a+0x$n)) \
    dsplibs.o | grep -oE 'dp_wrapper_create|RcFixed_Create' | sort -u | tr '\n' ' '
  echo
done
```

| module | rate path |
|---|---|
| `b103` (B103, V.21), `v22` (V.22/bis, B212), `v23`, `v32` (V.32/bis) | `dp_wrapper_create` → **native 8 kHz** |
| `v8` | no wrapper — **host rate, 9600** |
| `vpcm` (V.34/V.90/V.92) | no wrapper — **host rate, 9600** |
| `call` (CALLPROG) | own `RcFixed_Create` |

Fragment sizes: 160 samples (20 ms) for b103/v22/v23; 40 (5 ms = 12 symbols at
2400 baud) for v32.

`dp_wrapper_create` computes `frag * host_rate / dp_rate` (`imul`/`idiv`), and
`dp_wrapper_run` calls `RcFixed_Resample`. The rate-conversion factors are
plain rationals:

```sh
objdump -s -j .data --start-address=0x94e0 --stop-address=0x9560 dsplibs.o
```

`fixedRc_UpFact` / `fixedRc_DownFact` pair up to include **6/5 and 5/6** — the
8000↔9600 conversion. `RcFixed_Resample` is called from exactly four places:
`dp_wrapper_run`, `call_run`, `FAX_process`, `VOICE_process`.

## 6. Rate dependence lives above the pumps, not inside them

The only `_8000`/`_9600` coefficient pairs are reached from the host-rate
service layer:

```sh
for s in MTD1_COEF_8000 MTD1_COEF_9600 AUTOCOR_COEF_9600 AUTOCOR_COEF_7200; do
  echo -n "$s <- "
  objdump -d -r -j .text dsplibs.o \
    | awk -v s="$s" '/^[0-9a-f]+ </{fn=$2} $0 ~ ("R_386_(32|PC32)[ \t]+"s"$"){print fn}' \
    | sort -u | tr '\n' ' '; echo
done
```

`MTD*_COEF_*` → `DTMF_MTD_detect` only. `AUTOCOR_COEF_*` → `CID_FSD_demodulate`
only. `IIR2100_Coef_*` has no `.text` references at all (reached via a config
struct). **Nothing inside the V.22/V.23/V.32/B103 cores selects on sample
rate.**

Consequence for the eventual 8 kHz retarget: it is a configuration change for
those pumps, and real filter work only for V.8, DTMF, CID and call progress.
9600 is genuinely required only by V.34/V.90/V.92.

## 7. The stack resamples twice today

`d-modem.c:189` restricts the SIP wire to PCMU/PCMA **8000**, while
`d-modem.c:216` declares the pjmedia port at **9600**. So PJSIP upsamples
8000→9600, and `RcFixed` immediately downsamples 9600→8000 to feed a pump that
was native 8 kHz all along. Removing both stages is the payoff of the final
phase.

## 8. Pure x87, matching the modern `-m32` default

```sh
objdump -d -j .text dsplibs.o | grep -cE '\b(fld|fstp|fmul|faddp|fdiv)[a-z]*\b'  # 5300
objdump -d -j .text dsplibs.o | grep -cE '\b(movss|mulss|addss|movsd|mulsd)\b'   # 0
gcc -m32 -O -Q --help=target | grep mfpmath                                       # 387
```

Bit-exactness on float paths is therefore plausible, but not guaranteed —
instruction scheduling and spill points still differ between GCC 3.4.2 and a
modern compiler. Equivalence criteria are set per module class.

## 9. What a working call actually needs

From `modem.c`:

- `IS_FAST_DP` (line 1022) is `V34 || V90 || V92` **only**, so V.8 is never
  entered for B103/V.21/V.22/V.23/V.32.
- `modem_dial_start` (1586) requires `DP_CALLPROG` to originate.
- `modem_answer` (1568) does **not** — it sets `dp_requested = 0`, and
  `do_modem_change_dp` (1041) then takes `MODEM_DP(m)`.

So the shortest path to a real connection is **core plumbing + `b103`, answer
side only**, with the default datapump set to V.21/B103 via `AT+MS`.
Originating additionally needs CALLPROG. This is why the phase order is what
it is.

## 10. Per-TU attribution: what is and is not recoverable

`tools/tumap.py` recovers hard `.text` extents for only **19** of 281 TUs —
`ld -r` discarded the per-input section boundaries and they are not
recoverable from the symbol table.

A relocation-following approach was tried and **rejected**: the idea was that a
function references its own TU's data, so follow the reference and read off the
owner. It scored 33% against ground truth, because the data sections have only
38/33/13 anchored TUs, so resolving a data address yields the enclosing
*bracket*, not the TU. Recorded here so it is not attempted again.

What does work is naming. Smart Link's conventions are systematic and the
filenames survived, so `tools/tuattrib.py` matches C++ mangled class names and
C symbol prefixes against the TU list, filtered by address bracket:

| evidence | functions | agreement vs ground truth |
|---|--:|---|
| `class` / `prefix` (name-derived) | **1026** (58%) | **22/22 (100%)** |
| `name-only` (bracket-inconsistent) | 201 | 0/1 |
| `fill` (contiguity inference) | 95 | 0/8 |
| ambiguous / unattributed | 451 | — |

Name-derived attribution is authoritative. `fill` and `name-only` are hints
and must not be relied on — the tool keeps the two populations separate for
exactly this reason.

## 11. `dcr_process` really does take three arguments

`modem.c:79` declares `void dcr_process(void *, void *, int)` and `modem.c:677`
calls it with three. The blob agrees — after its `sub $0x2c,%esp` prologue it
reads exactly three incoming slots and no more:

```sh
objdump -d -j .text --start-address=0x100 --stop-address=0x338 dsplibs.o | head -20
#   mov 0x30(%esp),%ebx    <- arg0  dcr
#   mov 0x34(%esp),%ebp    <- arg1  buf
#   mov 0x38(%esp),%esi    <- arg2  len
```

`dcr_create` reads no arguments at all, matching `dcr_create()`. It allocates
**32 bytes** and initialises: `byte[0] |= 7`, `word[0x0a] = 3000`,
`long[0x14] = 5760`, `long[0x18] = 9600`, `long[0x1c] = 19200`. The 9600 is the
host sample rate, so the DC remover is one of the host-rate modules that will
need attention in the 8 kHz retarget.

`dcr_reset` is exported but never declared by slmodemd; it clears the three
accumulator fields at `+0x08`, `+0x0c`, `+0x10`.

## 12. Tier-1 differential testing is proven working

`make test` renames all 2350 defined symbols in the blob to `ref_*`, plus the
12 stateful imports, links it alongside the reconstruction and compares.

The import split is the part that matters. Sharing `modem_get_bits` between
the two implementations would make each consume the bits the other should have
seen — producing a plausible-looking waveform and a completely invalid result.
`tools/symmap.py` refuses to run if any import is unclassified, so a future
blob revision cannot silently introduce a shared stateful callback.

First module through the rig: the six G.711 companding routines
(`src/service/pcm.c`), tested **exhaustively** — all 65536 linear inputs and
all 256 code inputs, both directions, plus the saturation branch beyond
±32767. 161,028 checks, zero mismatches.

## 13. `FPM_sqrt` reads one element past the end of its own table

`FPM_sqrt` normalises its Q15 argument into `[0x4000, 0x7fff]`, then indexes
`FPM_sqrt_table` with `((mantissa + 64) >> 7) - 64`. Over the legal Q15 domain
that index runs 0..**192** — but the table has 192 entries, indices 0..191.

The overrun is not a corner case: **85 of the 32768 Q15 inputs hit it**, so it
fires in ordinary operation. What it reads is the first `unsigned short` of
`FPM_div_table`, which immediately follows at `.rodata 0x0c6a0`:

```sh
nm -S dsplibs.o | grep -E 'FPM_(sqrt|div)_table'
#   0000c520 00000180 R FPM_sqrt_table   <- 384 bytes = 192 entries
#   0000c6a0 00000100 R FPM_div_table
python3 tools/tabdump.py dsplibs.o --sym FPM_div_table --type u16 --count 1
#   32768
```

`FPM_div_table[0]` is 32768 — which is *exactly* the correct value for
`sqrt_table[192]`, because that index corresponds to `sqrt(256/256) = 1.0`
and `1.0` in Q15 is 32768. The out-of-bounds read returned the right answer by
coincidence, so the bug never produced a wrong result and was never noticed.

The real defect is simply that the table was declared one entry too short. The
reconstruction adds the missing 193rd entry, generated by the same formula as
every other entry, which is bit-exact with the original across the entire Q15
domain while being correct by construction rather than by luck.

Worth noting as a pattern: a table whose declared length is one short of what
its index expression can produce is exactly the kind of defect that survives
indefinitely when the adjacent data happens to be benign — and exactly the kind
that breaks when the tables are regenerated or reordered. The coefficient
generators required by this project's method would have caught it immediately.

## 14. Coefficient generators: first derivation recovered

`FPM_sqrt_table` is the first table to have its design recovered rather than
merely copied:

    table[i] = floor(32768 * sqrt((i + 64) / 256.0))     for i in [0, 193)

`FPM_sqrt_table_generate()` implements this and the unit test checks it against
all 193 extracted entries. This is the pattern every coefficient table should
follow: the generator is the maintainable artefact, the extracted bytes are the
reference the generator is checked against.

## 15. `dp_wrapper` — signature and layout (reconstruction pending)

The layer that lets an 8 kHz datapump sit under a 9600 Hz host. Established by
disassembly; recorded here so the reconstruction starts from facts.

**Signature**, from the call sites in `b103_create` / `v32_create` and the
argument use in `dp_wrapper_create` (`.text 0x5a80`, 0x2b6 bytes):

```c
void *dp_wrapper_create(void *dp_data, dp_process_fn process,
                        int dp_frag, int host_srate, int dp_srate);
```

`b103_create` passes `frag = 160` (20 ms at 8 kHz) and `dp_srate = 8000`,
with `host_srate` forwarded from `dp_operations::create`'s `srate` argument.
`v32_create` passes `frag = 40` (5 ms = 12 symbols at 2400 baud).

**Validation**, before anything is allocated: `dp_frag` must be non-zero and
**no greater than 192**; `host_srate` and `dp_srate` must both be non-zero.
Otherwise it returns NULL.

**State** is a single 2360-byte (`0x938`) allocation:

| offset | contents |
|---|---|
| `+0x000` | `dp_data` |
| `+0x004` | datapump `process` function |
| `+0x008` | back-pointer to the `struct dp` (filled by the caller) |
| `+0x00c` | resampler, host → dp direction |
| `+0x010` | resampler, dp → host direction |
| `+0x014` | start of the sample buffers |
| `+0x314` | host-side fragment = `dp_frag * host_srate / dp_srate` |
| `+0x318`, `+0x324` | same value, copied |
| `+0x628` | second buffer region |

**Two resamplers, one per direction**, confirmed by `dp_wrapper_delete`
(`.text 0x5a20`) calling `RcFixed_Delete` on both `+0x0c` and `+0x10`.

**Rate dispatch.** `dp_wrapper_create` compares the rate pair against literals
and picks a `RcFixed` mode directly — it never calls
`RcFixed_Check_Combination`, which is exported but dead (finding 10 in
`docs/deviations.md` D3). Pairs seen so far: 8000↔9600 (`0x1f40`/`0x2580`),
and both directions against 48000 (`0xbb80`). Equal rates short-circuit to no
resampling at all.

That last point is the mechanism behind the 8 kHz retarget: with the host at
8000 the `dp_srate == host_srate` branch is taken and the wrapper becomes a
pass-through, exactly as `RcFixed_Check_Combination(8000, 8000)` predicted.

### `dp_wrapper_run` — buffering (partially decoded)

`.text 0x5d40`, 0x23f bytes. Signature:

```c
int dp_wrapper_run(struct dp *dp, void *in, void *out, int count);
```

It reaches the wrapper state through `dp->dp_data` (`struct dp` offset
`+0x10`), so the state pointer is not passed directly.

**Two ring buffers**, each with a 16-byte descriptor followed by its data:

| | descriptor | data | capacity |
|---|---|---|---|
| output side | `+0x318` | `+0x328` | 768 bytes = 384 × int16 |
| input side | `+0x628` | `+0x638` | 768 bytes = 384 × int16 |

Descriptor fields: `+0x00` running total, `+0x04` write position, `+0x0c` read
position. Positions wrap modulo `2 * host_frag`, so each buffer is
double-buffered at the host fragment size — and 384 = 2 × 192 is exactly why
`dp_wrapper_create` rejects a `dp_frag` above 192.

**Reconstruction note.** The 192 is not an independent constant; it is a
consequence of the ring size. The reconstruction should derive it so the
relationship is visible and cannot drift:

```c
#define DPW_RING_BYTES   768                                  /* per ring   */
#define DPW_RING_SAMPLES (DPW_RING_BYTES / sizeof(short))      /* 384        */
#define DPW_MAX_FRAG     (DPW_RING_SAMPLES / 2)                /* 192        */
```

with the buffer declared as `short data[DPW_RING_SAMPLES]` and the validation
written against `DPW_MAX_FRAG`. Writing `192` in the check and `768` in the
declaration would leave two numbers that must agree with nothing saying so.

**Per-call flow:**

1. Take `n = min(count, host_frag, space in the input ring)` — the space term
   is computed against *both* the write position and a second index, so a
   partially-drained buffer cannot be overrun.
2. `sysdep_memcpy` that many samples into the input ring at its write
   position; advance the caller's `in` pointer and the running total; reduce
   the write position modulo `2 * host_frag`.
3. If the accumulated total is still below `host_frag`, return without calling
   the datapump — the wrapper buffers until a whole fragment is available.
4. Otherwise dispatch. If the host→dp resampler at `+0x0c` is non-NULL, take
   the resampling path at `.text 0x5f30`; if it is NULL (equal rates) call the
   datapump directly:

   ```c
   status = w->process(dp, &inbuf[w->in_rd], &outbuf[w->out_wr], host_frag);
   ```

   A non-zero return is latched as the call's status.

**Still to decode:** the resampling path at `.text 0x5f30`, the output copy
back to the caller's buffer, and the loop tail. Differential testing will need
a synthetic datapump installed on both sides, since `process` is a
caller-supplied function pointer — the harness's existing shim approach does
not cover indirect calls, so that is new machinery.

## 16. Bell 103 needs six `fpm_*` modules — a plan correction

The plan scheduled the `fpm_*` fixed-point framework with V.22 in phase 6, on
the assumption that Bell 103 was "FSK, no equaliser" and therefore standalone.
That is wrong. `B103FP_create` and `B103FP_delete` call:

| module | functions | bytes |
|---|---|--:|
| `fpm_agc.c` | `FPM_AGC_init/agc/Freeze/Release` | 868 |
| `fpm_fsd.c` | `FPM_FSD_init/demodulate/free` | 1059 |
| `fpm_fsm.c` | `FPM_FSM_init/modulate/delete` | 428 |
| `fpm_mrf.c` | `FPM_MRF_init/filter/free` | 742 |
| `fpm_mtd.c` | `FPM_MTD_create/detect/delete` | 513 |
| `fpm_tone.c` | `FPM_TONE_create/delete` and friends | ~3000 |

So phase 2 is not just `b103.c` + `B103*.c`; it is those six modules first.
Total for phase 2 is roughly 10–11 KB of code across ~10 translation units,
rather than the ~16 KB of B103 alone that the plan assumed — the count is
similar but the *composition* is different, and the `fpm_*` work lands earlier
than planned.

This is good news for the phases that follow: V.22 (phase 6) and the fax
modems (phase 9) both lean on the same framework, so most of it is paid for
once here. What remains for V.22 is the equaliser and timing-recovery half
(`fpm_fse`, `fpm_sre`, `fpm_pps`, `fpm_adeq`, `fpm_ecc`) plus the arithmetic
helpers.

`FPM_TONE` is the outlier at ~3 KB with 12 exported functions, and it has a
second, separate implementation alongside it (`TONE_create/generate/detect/…`
at `.text 0xaf690`, in `TONE.c`).

**Resolved: `FPM_TONE` is the real one; `TONE.c` is very nearly dead.**

```sh
objdump -d -r -j .text dsplibs.o \
  | awk '/^[0-9a-f]+ </{fn=$2} /R_386_PC32[ \t]+FPM_TONE_create$/{print fn}' | sort -u
```

`FPM_TONE_create` has **14** callers spanning every modulation — `B103FP_create`,
`V17RX_create`, `V22FP_create`, `v22_originate`, `v22_answer`,
`v23FP_tx_create`, `v23FP_rx_create`, `V29RX_create`, `V32FP_recreate`,
`fax_class1_create`, `CreateV23Modem`, `BwChDem_Create`, `SetToneDetect`, and
`FPM_FSM_init`.

`TONE_create` has **one** (`detector_create`), and `TONE_generate` has
**none at all**. So `TONE.c` survives only to serve `detector.c`, and its
generator half is unreachable. Reconstruct `fpm_tone.c` and treat `TONE.c` as
a low-priority curiosity.

### `FPM_FSM_init` — a scale factor worth explaining before reuse

`FPM_FSM_init(state, cfg)` copies the mark and space frequencies from an
8-byte `FPM_FSM_CFG`, then scales each by `0x471c` in Q14 before handing them
to the tone generator:

```
imul $0x471c, freq -> sar $14        i.e. freq * 18204 / 16384 = freq * 1.11108
```

That ratio is 10/9 to five figures, and 8000/7200 = 1.1111. The obvious reading
is a conversion from a 7200 Hz reference to the 8 kHz the datapumps run at,
which would make it exactly the kind of embedded rate assumption the 8 kHz
retarget has to find. It is **not confirmed** — it could equally be a
generator-specific normalisation — and it should be settled from
`FPM_TONE_create`'s use of the value rather than guessed, because if it *is* a
rate conversion it needs regenerating rather than copying.

## 17. Bell 103 / V.21's FSK core runs at **7200 Hz**, not 8000

Finding 5 established that the pumps present an 8 kHz interface to
`dp_wrapper`. That is true of the *interface*, but Bell 103 converts again
internally: its FSK core runs at **7200 Hz**.

Three independent pieces of evidence.

**1. The modulator config says so outright.**

```sh
python3 tools/tabdump.py dsplibs.o --sym FPM_FSM_CFG --type s16
#   1850, 1650, 24, 32767
```

1850 and 1650 Hz are the V.21 channel-2 space and mark frequencies. **24** is
samples per symbol — and 7200 / 300 baud = 24 exactly, where 8000 / 300 =
26.667. A 300 baud FSK modulator wants an integer symbol period, and only 7200
gives one.

**2. The frequency scaling composes to 7200.**

`FPM_TONE_create` converts Hz to a Q15 phase increment:

```
imul $0x8312, freq -> add $0x1000 -> sar $13      freq * 33554/8192 = freq * 4.096
```

4.096 = 32768/8000, so the tone generator assumes **8000 Hz**.

`FPM_FSM_init` pre-scales its frequencies before handing them over:

```
imul $0x471c, freq -> sar $14                     freq * 18204/16384 = freq * 1.11108
```

1.11108 = round(16384 × 10/9)/16384, and 10/9 = 8000/7200. Composing the two:

```
phase = f * (10/9) * (32768/8000) = f * 32768/7200
```

That is a phase increment correct for a **7200 Hz** sample rate. The 10/9 is
not a quirk of the modulator — it is the correction that retargets an
8000-assuming tone generator to 7200.

**3. `B103FP_create` installs a channel interpolator.**

`B103_CHAN_INTRP`, a 15-tap asymmetric FIR referenced only from
`B103FP_create`:

```
56, -104, 259, -583, 1225, -2768, 13089, 6543, -2521, 1368, -774, 414, -198, 83, -46
```

The two large adjacent taps (13089, 6543 — 0.80 and 0.40 in Q14) are the
signature of a fractional-delay filter rather than a symmetric lowpass.

**What is confirmed and what is not.** That the FSK core is clocked at 7200 is
confirmed by (1) and (2) — the samples-per-symbol constant and the phase
scaling agree independently. The exact mechanism by which 8000 becomes 7200,
and the precise role of `B103_CHAN_INTRP` in it, is **not** yet traced; that
needs reading how `B103FP_modem` and the Hdx state functions use it.

Note that `RcFixed` already offers 9:10 and 10:9 as modes 18 and 19, so the
ratio is one the resampler supports — but `dp_wrapper`'s rate table lists only
{8000, 9600, 48000}, so whatever B103 does internally, it does not go through
`dp_wrapper`.

**Consequence for the retarget.** The picture "host 9600 → pumps 8000, remove
the conversion and everything is native" is too simple for Bell 103. Moving the
host to 8000 removes the outer conversion, but the inner 8000 → 7200 stage
remains, because 7200 is what makes 300 baud come out as an integer. Recorded
as R-8 in `docs/rate_assumptions.md`.

## 18. `fpm_tone.c` and `FPM_phasor` — structure (reconstruction pending)

The tone generator every modulation depends on (finding 16). Decoded far
enough to record the shapes; not yet implemented.

### `FPM_phasor` — quarter-wave table lookup with interpolation

`.text 0x0a9300`, 211 bytes. Operates on a 8-byte phasor:

```c
struct fpm_phasor {
	unsigned short phase;	/* +0x00 accumulator            */
	short cos;		/* +0x02 output                 */
	short sin;		/* +0x04 output                 */
	unsigned short inc;	/* +0x06 phase increment        */
};
```

The lookup splits the 16-bit phase into a table index and a 5-bit fraction:

```
idx   = phase >> 5
frac  = phase - (idx << 5)          /* 0..31                      */
oct   = idx >> 8                    /* octant                     */
if (oct & 1) { frac = 32 - frac; idx = ~idx; }   /* mirror        */
idx  &= 0xff
v     = cos_table[idx] + ((cos_table[idx+1] - cos_table[idx]) * frac >> 5)
out   = (v * cos_sign[oct]) >> 15
```

So the tables (`FPM_cos_table`, `FPM_sin_table`, both 257 × u16 at `.rodata
0xcde0` and `0xcbc0`) hold one octant, mirrored and sign-flipped via
`FPM_cos_sign` — the classic quarter-wave trick. The 257th entry exists so the
interpolation can read `idx+1` without a bounds check.

### `FPM_TONE_*` — the accessors are trivial, the state is not

```c
FPM_TONE_set_freq(state, hz)  -> state[0x26] = (hz * 0x8312 + 0x1000) >> 13
FPM_TONE_set_scale(state, s)  -> state[0x02] = s
```

`state[0x26]` is the phase increment and `state[0x02]` the output scale, both
confirmed by `FPM_TONE_generate`, which copies phase (`state[0x24]`) and
increment into a local phasor, calls `FPM_phasor` per sample, and scales:

```
out[i] = (state[0x02] * phasor.sin) >> 14
```

`FPM_TONE_create` copies a 36-byte config (`FPM_TONE_CFG`) into the state and
derives the initial phase increment with the same 0x8312 scaling — the 8 kHz
assumption recorded as R-9.

Beyond the generator loop, `FPM_TONE_generate` reads `state[0x28]` and
`state[0x04]` and branches — an envelope or duration counter, not yet decoded.
That, and the detector half (`FPM_TONE_detect`, `_find_rev`, `_filter`), remain
to do.

## 19. `FPM_TONE` is the V.25 answer tone generator, reversals and all

`FPM_TONE_generate`'s post-loop branch is not an envelope. It is a periodic
**180° phase reversal**, and the default config is literally the ITU-T V.25
answer tone.

```sh
python3 tools/tabdump.py dsplibs.o --sym FPM_TONE_CFG --type s16
#   2100, 27852, 450, 24576, 328, 1, 30720, 0, -12224, ...
```

- **2100** — the V.25 answer tone frequency, in Hz
- 27852 — output scale, ≈0.85 in Q15
- **450** — the reversal period

V.25 and V.8 specify ANSam as 2100 Hz reversing phase every **450 ms**, which
is what disables echo cancellers in the network so a modem can use the return
path.

The mechanism, from `.text 0x0aadb9`:

```
counter = state[0x28] + (count >> 3)
if (state[0x04] > 0 && counter >= state[0x04]) {
        state[0x28] = 0;
        phase += 0x4000;                  /* half of a 0x8000 cycle = 180° */
        if (phase > 0x7fff) phase -= 0x4000;
} else {
        state[0x28] = counter;
}
```

`0x4000` is exactly half the `0x8000` phase cycle, so the hop is 180°, not the
90° a first reading of the constant suggests.

**The `>> 3` is another 8 kHz assumption.** The counter advances by
`count / 8` per call, so a threshold of 450 is reached after 3600 samples —
and 3600 samples is 450 ms only at 8000 Hz. The config value is the ITU
figure in milliseconds *because* the scaling assumes 8 kHz. Recorded as R-10.

The detector counterpart exists too: `FPM_TONE_find_rev` is called from
`RxHdxPhsReversal`, the half-duplex receiver's phase-reversal detector.

**Reconstruction note.** An earlier reading of this code concluded the branch
was unreachable, because `objdump` filtering had hidden
`lea 0x0(%ebp,%ebx,1),%eax` — the instruction that computes the counter. A
disassembly filter that drops `lea` as "just padding" is unsafe: GCC uses `lea`
for arithmetic constantly. Filter on address ranges, not opcodes.

### `FPM_TONE_create` also builds a Goertzel detector

`FPM_TONE` is not only a generator. `FPM_TONE_create` initialises a
correlator alongside the oscillator, which is what `FPM_TONE_detect` and
`FPM_TONE_find_rev` run.

From `.text 0x0aaaa4`, after the phase increment is stored:

```
state[0x36] = 0x4000                      /* 1.0 in Q14                     */
state[0x38] = 0x4000
state[0x3a] = -(2 * phasor.cos)           /* Goertzel coefficient, -2cos(w) */
state[0x3c] = (state[0x0c] * state[0x0c]) >> 16
state[0x3e] = -(phasor.cos * state[0x0c]) >> 14
state[0x40 .. 0x50] = 0                   /* accumulators                   */
```

`-2·cos(ω)` at `+0x3a` is the Goertzel recurrence coefficient; `+0x36` and
`+0x38` are unity in Q14, and `+0x40` upward are the running accumulators the
recurrence updates. `phasor.cos` here comes from the same `FPM_phasor` call
that derives the increment, so the detector is tuned to exactly the frequency
the generator produces — one config drives both halves.

A second loop then fills two buffers, `state[0x2c]` and `state[0x30]`, of
`state[0x0e]` entries, calling `FPM_phasor` per entry and scaling by
`2 * phasor.cos >> 14`. That looks like a pre-computed reference waveform for
correlation, but it is **not yet confirmed** — the source buffer `state[0x10]`
has not been traced to where it is filled.

### A second, damped resonator

`FPM_TONE_create`'s tail (`.text 0x0aab90`) sets up more:

```
state[0x52 .. 0xf0] = 0                  /* 80-entry working array        */
state[0xfc] = &state[0x36]               /* pointer to the first section  */
state[0x100 .. 0x106] = 0
```

then calls `FPM_phasor` a *second* time with phase 0 — so `cos = 1.0` — and
writes a second coefficient block through the pointer at `state[0xf4]`:

```
blk[0] = 0x4000                          /*  1.0 in Q14                   */
blk[1] = 0x4000
blk[2] = -(2 * cos)                       /* -2 r cos(w), with cos = 1.0   */
blk[3] = 0x3afb                           /*  0.9216                       */
blk[4] = (cos * -31457) >> 14             /* -1.92                         */
```

Those constants identify it: 0.9216 = 0.96² and −1.92 = −2 × 0.96, so this is
a **damped** resonator with pole radius **r = 0.96** rather than the
undamped Goertzel of the first section. Tuned at ω = 0 as initialised, which
makes it an energy or envelope follower; a caller retunes it via
`FPM_TONE_set_freq`.

So one `FPM_TONE` object carries an oscillator, an exact-frequency Goertzel,
and a leaky resonator — which is why a single config serves generation,
detection and the phase-reversal search.

### `FPM_TONE_create` fully decoded

```c
FPM_TONE_create(state, cfg)

    owned = 0
    if (state == NULL) {                       /* .text 0x0aacd9 */
            state = sysdep_malloc(0x108)
            owned = 1
    }
    if (cfg == NULL)                           /* .text 0x0aac89 */
            copy FPM_TONE_CFG (36 bytes) into state[0x00 .. 0x20]
    else
            copy cfg (36 bytes) into state[0x00 .. 0x20]

    if (owned && (short)state[0x14] > 0) {     /* .text 0x0aac3c */
            state[0x2c] = malloc(state[0x14] * 2)
            state[0x30] = malloc((state[0x14] + state[0x20]) * 2)
            state[0xf4] = malloc(10)           /* 5 shorts: resonator coeffs */
            state[0xf8] = malloc(8)            /* 4 shorts: its accumulators */
    }

    /* oscillator */
    state[0x24] = 0                            /* phase                     */
    state[0x28] = 0                            /* reversal counter          */
    state[0x26] = (state[0x00] * 0x8312 + 0x1000) >> 13    /* increment      */

    /* exact-frequency Goertzel, tuned by a phasor run at the same frequency */
    state[0x36] = state[0x38] = 0x4000
    state[0x3a] = -(2 * phasor.cos)
    state[0x3c] = (state[0x0c] * state[0x0c]) >> 16
    state[0x3e] = -(phasor.cos * state[0x0c]) >> 14
    state[0x40 .. 0x50] = 0
    state[0x52 .. 0xf0] = 0                    /* 80-entry working array     */

    /*
     * Correlation reference.  state[0x10] is a POINTER to a source waveform;
     * each sample is modulated by a cosine running at the tone frequency.
     * The loop bound is state[0x14] -- the same field that sized the buffer
     * and gated its allocation, not state[0x0e].
     */
    p.inc = p.phase                        /* now advance at the tone rate */
    for (i = 0; i < (short)state[0x14]; i++) {
            FPM_phasor(&p)
            ((short *)state[0x30])[i] = 0
            ((short *)state[0x2c])[i] =
                    (2 * ((short *)state[0x10])[i] * p.cos) >> 14
    }

    /* damped resonator, r = 0.96, via the pointer at state[0xf4] */
    state[0xfc] = &state[0x36]
    state[0x100 .. 0x106] = 0
    run FPM_phasor once more from phase 0, so cos = 1.0, then:
            blk = state[0xf4]
            blk[0] = blk[1] = 0x4000
            blk[2] = -(2 * cos)
            blk[3] = 0x3afb                    /* 0.96^2                     */
            blk[4] = (cos * -31457) >> 14      /* -2 * 0.96                  */
            ((short *)state[0xf8])[0..3] = 0
```

**Ownership.** The four buffers are allocated only when `FPM_TONE_create`
allocated the object itself *and* `state[0x14]` is positive. A caller
supplying its own object supplies its own buffers — the contract recorded in
finding 20. `FPM_TONE_delete` must mirror that, or it double-frees.

**Config layout** (36 bytes, `FPM_TONE_CFG` = the V.25 answer tone):

| offset | value in `FPM_TONE_CFG` | role |
|---|--:|---|
| `+0x00` | 2100 | frequency, Hz |
| `+0x02` | 27852 | output scale, Q15 |
| `+0x04` | 450 | phase-reversal period, 8-sample units |
| `+0x0c` | — | resonator damping input |
| `+0x10` | *(relocated)* | **pointer** to the source waveform |
| `+0x14` | — | buffer size *and* reference length; zero suppresses allocation |
| `+0x20` | — | extra length added to the second buffer |

**How the phasor is used to derive coefficients.** Before the first
`FPM_phasor` call the local phasor is set up as `phase = increment, inc = 0`,
so the call evaluates cos and sin *at* the increment — i.e. at
`omega = 2*pi*f/8000` — without advancing. That is exactly what the Goertzel
coefficient `-2*cos(omega)` needs. The fill loop afterwards sets `inc = phase`
so subsequent calls sweep at the tone frequency.

Two errors in the first version of this pseudocode, corrected above and
recorded because both would have produced a plausible but wrong module: the
fill loop is bounded by `state[0x14]`, not `state[0x0e]`, and `state[0x10]` is
a pointer to a source waveform rather than a scalar.

**How the second error happened, and the tool fix it prompted.** Dumping
`FPM_TONE_CFG` as `int16` showed `-12224` at `+0x10`, which reads perfectly
well as a scalar. It is not: there is an `R_386_32` relocation there, so the
stored value is only the *addend* and the linker supplies the rest. A probe
against the reference confirmed the field holds a valid pointer
(`0x810c300`), not `0xd040`.

In a relocatable object any table may contain embedded pointers, and dumping
it as scalars hides them silently — the numbers look plausible and the
misreading only surfaces much later. `tools/tabdump.py` now checks the dumped
range against the relocation table and prints a warning naming each offset and
target:

```
 * WARNING: 1 relocation(s) inside this range -- the values below are
 * ADDENDS, not final values. ...
 *   +0x0010  R_386_32       .rodata
```

Worth applying retrospectively: any table already extracted should be
re-checked, though the coefficient banks are pure data and unaffected.

Implementation is now mechanical.

## 20. `B103FP_create` — partial decode

`.text 0x08e690`, 2151 bytes. Signature `B103FP_create(state, cfg)`.

Opens by copying 28 bytes from the config (`B103_CFG`) into the state at
offsets 0x00–0x18, then works through `state[0x50]`.

Through that pointer it builds a half-duplex transmit context:

```
timing = state[0x0c] / 20               /* signed divide, magic 0x66666667 */
if (timing < 700) timing = 700          /* clamped                         */
ptr[0x02] = timing
ptr[0x08] = TxHdxStartB103              /* state-machine entry point       */
ptr[0x04] = ptr[0x0c] = ptr[0x14] = 0
ptr[0x18] = ptr[0x1c] = ptr[0x20] = 0
```

so `state[0x50]` is a transmit state block with a function-pointer entry, and
`TxHdxStartB103` is where the half-duplex transmitter begins. `FPM_TONE_CFG`
fields are then copied both onto the stack and into that block — the tone
configuration is built per-instance rather than shared.

### Ownership: `B103FP_create` allocates everything — two earlier readings were wrong

Checked directly, and both of my previous conclusions about this were
mistaken. Recording the corrections because the wrong version would have led
to a reconstruction that double-frees or leaks.

**Wrong reading 1:** that `state[0x50]` "must be non-NULL or the function
returns early". It is the opposite — NULL is the *allocate* case:

```
8e6e0:  mov 0x50(%esi),%ecx
8e6e3:  test %ecx,%ecx
8e6e5:  je 8ed70            ->  malloc(0x24); state[0x50] = it; rejoin
```

**Wrong reading 2:** that the allocations "must be in `b103_create`".
`b103_create` has exactly **one** `sysdep_malloc`, of 0x348 bytes, which is its
own datapump state (the leading 0x14 bytes of which are the `struct dp`).
`B103FP_create` has **six**:

| size | stored at | note |
|--:|---|---|
| 0x58 (88) | returned | its own state, when called with a NULL state |
| 0x24 (36) | `state[0x50]` | half-duplex TX block |
| 0x100 (256) | `state[0x54]` | holds the three pointers below |
| 0x144 (324) | `+0xe8` of that block | |
| 0x144 (324) | `+0xec` | |
| 0x54 (84) | `+0xf0` | |

So `B103FP_create` owns its whole tree, and `b103_create` calls it as
`B103FP_create(NULL, &cfg)` — the "pass NULL to allocate" idiom, used at two
levels: once for the object itself and once per sub-block.

**The 0x144 guess was wrong.** All four `FPM_TONE_create` call sites inside
`B103FP_create` pass **NULL** as the state, so `FPM_TONE` allocates its own:

```
aacd9:  movl $0x108,(%esp)
        call sysdep_malloc
        mov %eax,%esi
        mov $0x1,%edx           /* ownership flag */
        jmp back into the main path
```

**0x108 = 264 bytes**, which matches the highest observed field offset
(`+0x106`) exactly — so the `FPM_TONE` object is fully accounted for.

It has two further allocations, of 0xa and 0x8 bytes, and they are **guarded by
the ownership flag**:

```
allocated_by_us = (edx != 0)
if (allocated_by_us && state[0x14] > 0)  -> allocate the two buffers
```

So the contract is: a caller that supplies the object also supplies its
buffers; a caller that passes NULL gets both. That resolves the open question
from finding 19 — the buffers at `state[0x2c]`, `state[0x30]`, `state[0xf4]`
and `state[0xf8]` are these 0xa and 0x8 blocks, self-allocated.

### The allocation idiom, library-wide

| function | allocations when passed NULL |
|---|---|
| `b103_create` | 0x348 — its own state, `struct dp` as the leading 0x14 |
| `B103FP_create` | 0x58 own state, 0x24, 0x100, 2 × 0x144, 0x54 |
| `FPM_TONE_create` | 0x108 own state, plus 0xa and 0x8 buffers if it owns the object |

Every level uses the same "pass NULL to allocate" idiom with an ownership flag
so the matching `_delete` frees only what it created. The reconstruction must
reproduce that flag, not just the allocation — a version that always frees
would double-free whenever a caller supplies its own storage.

The two 0x144 blocks and the 0x54 remain unattributed; they are reached at
`+0xe8`, `+0xec` and `+0xf0` of the 0x100 block and are candidates for the FSD
and MRF working buffers, since those are the modules `B103FP_create` inits
against `state[0x54]`.

**Possible rate dependency, unconfirmed.** The `/20` with a floor of 700 has
the shape of a duration in samples or milliseconds. If `state[0x0c]` carries a
rate or a sample count, the divisor moves with it. Logged as a candidate
rather than a confirmed entry in `docs/rate_assumptions.md`, because
`B103_CFG`'s field meanings are not yet established —
`B103_CFG = { 2, 0, 0, 0, 0, 0, 14000, 0, 1, 0, 0, 0, 3200, 0 }` as int16, and
`state[0x0c]` is the 14000.

14000 / 20 = 700, exactly the clamp floor, so the clamp is a no-op for the
default config and only bites if a caller lowers the value.

## 21. The `fpm_*` dependency web

Mapped while scoping `FPM_AGC`. The six modules Bell 103 needs are not
independent — they bottom out in a small set of arithmetic primitives, and the
order they have to be reconstructed in follows from that.

```
FPM_AGC_agc ──> FPM_div  ──> FPM_div_table
            └─> FPM_rms  ──> FPM_sqrt_dp ──> FPM_sqrt_table   (done)
FPM_FSM_init ──> FPM_TONE_create ──> FPM_phasor                (done)
```

**Decoded, trivial:**

```c
FPM_AGC_Freeze(state)   ->  state[0x28] = 1
FPM_AGC_Release(state)  ->  state[0x28] = 0
FPM_AGC_init(state, cfg, reset)
        state[0x1c] = 0
        copy 24 bytes of cfg into state[0x00 .. 0x14]
        if (reset) { state[0x18] = 1; state[0x20] = state[0x24] = state[0x26] = 0; }
        state[0x28] = 0
        state[0x20] = 0
```

**`FPM_rms(samples, count)`** accumulates `(x * 910 >> 15) * x` per sample and
takes `FPM_sqrt_dp` of the total. 910/32768 = 0.02777 ≈ 1/36, so it computes
`sqrt(Σ x² / 36)` — the /36 is headroom, keeping the sum inside 32 bits for up
to 36 full-scale samples.

**`FPM_sqrt_dp(x)`** is the 32-bit sibling of `FPM_sqrt`, sharing
`FPM_sqrt_table` and the same normalise-index-interpolate shape. It
left-normalises until `x > 0x1fffffff`, then takes `x >> 15` as the mantissa.

**Edge case to check before reconstructing it.** After normalisation
`x ∈ [0x20000000, 0xffffffff]`, so `x >> 15` reaches `0x1ffff` — which does not
fit the `unsigned short` the mantissa is stored in, and wraps for
`x ≥ 0x80000000`. That is the same shape of latent defect as `FPM_sqrt`'s
one-past-the-end read (D1), and it needs the same treatment: establish whether
callers can reach it before deciding whether to reproduce or guard. `FPM_rms`
is the only caller found so far, and its accumulator is bounded by the /36
scaling, so it may well be unreachable in practice.

**Reconstruction order that falls out:** `FPM_sqrt_dp` → `FPM_rms` →
`FPM_div` → `FPM_AGC`. Each is small (143, 62, 150, 868 bytes) and the first
three are exhaustively or near-exhaustively testable.

## 22. `FPM_AGC` — structure (superseded in part by finding 28)

Three of the four functions are trivial and decoded; the fourth is the work.

```c
FPM_AGC_Freeze(state)   ->  state[0x28] = 1     /* hold the current gain */
FPM_AGC_Release(state)  ->  state[0x28] = 0
FPM_AGC_init(state, cfg, reset)
        state[0x1c] = 0
        copy 24 bytes of cfg into state[0x00 .. 0x14]
        if (reset) { state[0x18] = 1; state[0x20] = state[0x24] = state[0x26] = 0; }
        state[0x28] = 0
        state[0x20] = 0
```

### `FPM_AGC_agc(state, samples, count)` — `.text 0x0a6750`, 566 bytes

It is a **block** AGC, not per-sample. The entry computes how to divide the
input into measurement blocks:

```
blocksize = (unsigned short)state[0x0a]
blocks    = count / blocksize
tail      = count % blocksize
if (tail < blocksize / 2)
        tail += blocksize      /* fold a short tail into the last block   */
```

so a remainder shorter than half a block is absorbed rather than measured on
its own — which stops a stray few samples producing a wild RMS and yanking the
gain.

The loop then calls `FPM_rms` per block and compares the result against two
thresholds from the config, `state[0x02]` and `state[0x04]`, with the
behaviour gated on `state[0x24]`. That is the classic
low-threshold/high-threshold AGC shape: below the low mark increase gain,
above the high mark decrease it, in between hold.

**Config fields identified so far** (from `AGCb103_CFG`, 24 bytes copied at
init):

| offset | value in `AGCb103_CFG` | use |
|---|--:|---|
| `+0x00` | 16384 | compared against RMS (a target or reference), 1.0 in Q14 |
| `+0x02` | 10 | low threshold |
| `+0x04` | 80 | high threshold |
| `+0x06` | 1000 | |
| `+0x08` | 1 | |
| `+0x0a` | 36 | measurement block size, in samples |
| `+0x0c` | *(relocated)* | **pointer** into `.data` |
| `+0x10` | *(relocated)* | **pointer** into `.data` |
| `+0x14` | 158 | |

The two pointers were found by the relocation audit prompted by
`FPM_TONE_CFG` (see the note under finding 18). Dumped as scalars they read as
30736 and 30732, which look like plausible coefficients; they are addends.
`FPM_AGC_init` copies 24 bytes, so both are carried into the state and the
gain update presumably indexes through them.

Note the block size of **36** matches `FPM_rms`'s 1/36 scaling exactly — the
accumulator headroom was sized for precisely this block length, which is a
good cross-check that both readings are right.

**Runtime state:**

| offset | use |
|---|---|
| `+0x18` | set to 1 by `init` when reset |
| `+0x20`, `+0x24`, `+0x26` | gain//hysteresis state, cleared on reset |
| `+0x28` | freeze flag |

Two details in this section were wrong and are corrected in finding 28:
`FPM_AGC_Release` also clears the level estimate, and `state[0x20]` is that
estimate rather than a gain. Read finding 28 in preference to this one.

## 23. The 7200 Hz question, sharpened — and every conversion candidate excluded

Task 11 asked what converts 8000 to 7200 inside Bell 103. The answer so far is
**nothing found**, and that is now a specific, well-bounded puzzle rather than
a vague one.

### Confirmed: the FSK core is 7200 Hz

Two independent facts, both now read directly rather than inferred.

**`FPM_FSM_modulate` emits exactly 24 samples per bit.** The per-bit loop calls
`FPM_TONE_generate` once per sample, 24 times, and accumulates 24 into a total
it returns:

```
per bit:  set_freq(tone, prescaled[bit]); set_scale(tone, scale)
          total += 24
          repeat 24 times: FPM_TONE_generate(tone, out++, 1)
```

Integer, no fractional accumulator. At 300 baud that is 7200 Hz exactly;
8000 would need 26.667.

**The tone frequencies are prescaled by 10/9.** `FPM_FSM_init` stores
`cfg[0] * 0x471c >> 14` and `cfg[1] * 0x471c >> 14`, and `FPM_FSM_modulate`
passes those to `FPM_TONE_set_freq`, which assumes 8000. Composed, that yields
phase increments correct for 7200. With `FPM_FSM_CFG`'s 1850/1650 — the V.21
channel-2 pair — the prescaled values are 2055.6 and 1833.3, which are only
the right tones if the stream is clocked at 7200.

### Excluded

- **`B103_CHAN_INTRP` is not a resampler.** It is passed to `FPM_FSD_init` as
  a 15-tap coefficient array with a tap count of 15 alongside `B103_IIR_LPF` —
  it is the demodulator's input filter.
- **`RcFixed` is not involved.** Its only callers are `dp_wrapper_run`,
  `call_run`, `FAX_process` and `VOICE_process` (finding 5). Nothing in the
  B103 path calls it.
- **`b103_process` does not resample.** Its entire call graph is
  `B103FP_modem`, `modem_get_bits`, `modem_put_bits`, `modem_set_param` and
  the debug printf.
- **`dp_wrapper` is told 8000.** `b103_create` passes `dp_srate = 8000` and
  `dp_frag = 160`, giving `host_frag = 160 * 9600 / 8000 = 192` — which is
  exactly `DPW_MAX_FRAG`, so b103 uses the ring to its limit.

### What that leaves

Either the conversion lives somewhere not yet read — `B103FP_modem` (0x307
bytes) and the `TxHdx*`/`RxHdx*` state functions are the remaining
unexamined code on this path — or there is none, and the modem runs 10/9 fast
relative to the standard.

The second possibility should not be dismissed on plausibility alone. It would
mean two Smart Link modems interoperate happily while neither matches a
standards-compliant peer, which is the kind of defect that survives if it is
never tested against third-party hardware. It would also be invisible to every
test in this project, since we compare against the blob and the blob would be
consistently wrong.

**This is exactly what tier-3 interop testing exists to catch** (SpanDSP, task
5, currently deferred). If B103 is 10/9 fast, a SpanDSP V.21 peer will fail to
train against it while our differential tests stay green. That makes task 5
worth pulling forward once B103 is reconstructed, rather than leaving it to
phase 9.

## 24. Task 11 RESOLVED — `FPM_MRF` is the rate converter

`FPM_MRF` is a **multi-rate filter**, not the "matched root filter" the
abbreviation suggests. That misreading is why the converter went unfound for
several passes: it was excluded on the strength of an assumed expansion of
three letters.

`FPM_MRF_init` computes taps-per-phase directly, which settles the semantics:

```
state[0x16] = (short)(cfg[0x08] / cfg[0x00])      /* taps / branches */
```

`B103FP_create` initialises two of them:

| | factors | taps | phases × taps | conversion |
|---|---|--:|---|---|
| TX, `B103_MRF_FILT_TX` | 10 : 9 | 270 | 10 × 27 | **7200 → 8000** |
| RX, `B103_MRF_FILT_RX` | 3 : 10 | 90 | 3 × 30 | **8000 → 2400** |

### The complete Bell 103 rate chain

```
host 9600 ──RcFixed mode 3──> 8000 ──┬── RX: FPM_MRF(3:10) ──> 2400 ──> FPM_FSD
                                     │
      8000 <──FPM_MRF(10:9)── 7200 <─┴── TX: FPM_FSM at 24 samples/symbol
```

Every number now agrees. The FSK modulator runs at **7200** (24 samples/symbol
× 300 baud) with tones prescaled by 10/9 so an 8000-assuming tone generator
produces the right phase increment. The demodulator runs at **2400** — 8
samples per symbol at 300 baud, a sensible FSK demod rate. `FPM_MRF` bridges
both to the 8000 the datapump interface uses.

### The worry in finding 23 is resolved

That finding raised the possibility that no conversion existed and the modem
therefore ran 10/9 fast relative to the standard — invisible to differential
testing because the blob would be consistently wrong. **It does not.** The
conversion is present and exactly 10/9.

The tier-3 argument still holds on its own merits, but this specific alarm is
stood down.

### For the reconstruction

`FPM_MRF` joins `FixedRC` as a second, independent polyphase resampler in the
library — different implementation, different layer, same job. It is one of
the six `fpm_*` modules Bell 103 needs, and now clearly among the more
important: it carries both rate conversions in the datapump.

Its coefficient banks (`B103_MRF_FILT_TX`, 270 × int16; `B103_MRF_FILT_RX`,
90 × int16) should be run through `tools/rcfilter.py`'s analysis once the
storage order is established — the design rule recovered for `FixedRC`
(cutoff ≈ 0.94 of the binding Nyquist) is worth checking against these.

## 25. `FPM_MRF_filter` — partial decode

`.text 0x0a8ed0`, 533 bytes. The polyphase resampling core. Signature, from
the stack slots read:

```c
FPM_MRF_filter(struct fpm_mrf *state, const short *in, short *out, short count)
```

with `state` at `0x3c(%esp)`, `in` at `0x40`, `out` at `0x44` and `count` at
`0x48` after the prologue.

**State fields it reads**, beyond the config copied by `init`:

| offset | role |
|---|---|
| `+0x10` | compared against `count` to decide whether a full pass runs |
| `+0x12` | running index, saved back across calls |
| `+0x14` | limit paired with `+0x12` |
| `+0x16` | history length (`taps / branches`) |
| `+0x18` | history buffer |

**The history is circular, incremented branchlessly:**

```
next = idx + 1
in_range = (next < state[0x16])          /* setl                        */
idx = next & -(in_range)                 /* next if in range, else 0    */
history[idx] = *in++
```

`neg` of a `setl` result to form a 0/-1 mask, then `and` — no branch. Worth
recognising rather than puzzling over: GCC 3.4.2 emits this shape wherever a
wrap-to-zero appears, and it will recur in the other `fpm_*` modules.

Note this differs from both resamplers reconstructed so far: `FixedRC` uses a
flat window compacted periodically, `GenericIIR` a downward-growing one with
the same trick reversed, and `FPM_MRF` a genuine circular buffer. Three
different history strategies in one library, each chosen for its access
pattern.

### The algorithm, fully decoded

```
L = state->branches      (cfg[0x00])   /* interpolation factor  */
M = state->decimate      (cfg[0x02])   /* decimation factor     */
phase = state[0x12]      widx = state[0x14]      need = state[0x10]

while (count >= need) {
        /* consume `need` inputs into the circular history */
        repeat need times:
                widx = (widx + 1 < hlen) ? widx + 1 : 0
                history[widx] = *in++
        count -= need

        /* one output: inner product over the whole circular history,
           newest first, with coefficients strided by L from `phase`   */
        acc = 0
        c = &coeff[phase]
        for (k = widx;      k >= 0;    k--)  { acc += history[k] * *c; c += L; }
        for (k = hlen - 1;  k > widx;  k--)  { acc += history[k] * *c; c += L; }
        *out++ = acc >> 15

        /* advance the phase; each wrap past L costs one more input */
        phase += M
        need = 0
        while (phase >= L) { phase -= L; need++; }
}
state[0x12] = phase;  state[0x14] = widx;  state[0x10] = need
```

**The coefficient stride is `L` shorts**, set up once as `2 * branches` bytes.
So the bank is stored *phase-interleaved* — `coeff[k * L + p]` — the opposite
of `FixedRC`, which is phase-major (`coeff[p * N + k]`). Two polyphase
resamplers in one library with opposite storage conventions; neither layout can
be assumed from the other.

**Output rate is `L / M`.** Each output advances the phase by `M` and consumes
one input per wrap past `L`. For Bell 103's transmit filter, L=10 and M=9 gives
10 outputs per 9 inputs — 7200 → 8000 exactly. For receive, L=3 and M=10 gives
3 per 10 — 8000 → 2400.

**Scaling is Q15** (`sar $0xf`), unlike `FixedRC`'s Q14. Another place the two
resamplers differ.

**State that persists across calls:** `phase`, the history write index, and
`need` — the number of inputs still owed before the next output. That last one
is why fragment boundaries matter: a reconstruction that recomputed `need` from
scratch each call would drift, and only under uneven chunking.

## 26. `FPM_iir_filt` — the shared biquad cascade

`.text 0x0a8a50`, 191 bytes. Six callers, including **both** remaining Bell 103
cores (`FPM_FSD_demodulate` and `FPM_MTD_detect`) as well as `band_pass`,
`BwChDem_Progress`, `CID_MTD_detect` and `DTMF_MTD_detect`. So it is the next
thing to reconstruct: it unblocks two modules at once and is reused across
CID and DTMF later.

```c
short FPM_iir_filt(short x, const short *coeff, short *state, short sections)
```

A **direct-form II biquad cascade**, Q14, with **5 coefficients and 2 state
words per section**:

```
per section:
        w1  = state[0]
        acc = x + (coeff[0] * w1 + 0x2000) >> 14
        ff  =     (coeff[1] * w1)          >> 14
        w2  = state[1]
        state[0] = w2                        /* shift the delay line     */
        acc += (coeff[2] * w2 + 0x2000) >> 14
        ff  += (coeff[3] * w2)          >> 14
        w   = saturate_s16(acc)              /* recursive node only      */
        state[1] = w
        x = (short)(ff + ((coeff[4] * w) >> 14))   /* feeds the next     */
        coeff += 5
        state += 2
```

Two details worth preserving:

- **Only the recursive node saturates.** `acc` is clamped to
  `[-0x7fff, 0x7fff]` — note the negative limit is `-0x7fff`, not `-0x8000` —
  before being stored as state. The feedforward sum is merely truncated to 16
  bits. Clamping both, or neither, would be wrong in opposite ways.
- **Rounding is asymmetric.** The two terms feeding `acc` add `0x2000` before
  the shift; the two feeding the feedforward sum do not. That is not a
  symmetry worth "fixing".

### This explains the 5-word blocks seen earlier

`FPM_TONE_create` allocates 10 bytes at `state[0xf4]` and 8 at `state[0xf8]`,
and writes exactly five coefficients into the first (finding 18). Those are
**one biquad section and two sections' worth of state** — the damped resonator
at r = 0.96 is a single biquad run through this function. The odd-looking
allocation sizes were the cascade layout all along.

`FPM_MTD_detect` likewise calls it twice per sample: once with `COEF_DC` and
one section over `state[0x10]`, then once with the tone bank
(`state[0x00]`, `state[0x0c]`, `state[0x04]` sections). Its output feeds two
leaky energy integrators with `alpha = 820/32768` (~0.025) against
`31948/32768` (~0.975) — a 40-sample time constant.

## 27. `FPM_FSD_demodulate` — partial decode

`.text 0x0a79f0`, 745 bytes — the largest single function on the Bell 103 path.

```c
short FPM_FSD_demodulate(struct fpm_fsd *state, const short *in,
                         <arg2>, short count);
```

The compiler hoists nearly every state field into stack slots before the loop,
so the entry is 200 bytes of loads. Mapped so far:

| stack slot | field |
|---|---|
| `0x50` | `state[0x00]` FIR coefficients |
| `0x68` | `state[0x04]` FIR tap count |
| `0x60` | `state[0x06]` **discriminator delay** |
| `0x4c` | `state[0x08]` IIR coefficients |
| `0x64` | `state[0x0c]` IIR sections |
| `0x48` | `state[0x24]` FIR history |
| `0x44` | `state[0x2c]` IIR history |
| `0x40` | `state[0x1c]` third buffer |
| `ebp` | `state[0x28]` FIR write index |

### The input FIR

Same shape as `FPM_MRF_filter`: a circular history with the branchless wrap,
walked backward in two loops (write index down to 0, then top down to write
index + 1), accumulating into a Q15 result.

```
idx = (idx + 1 < taps) ? idx + 1 : 0
history[idx] = *in++
acc = 0
for (k = idx;      k >= 0;   k--)  acc += history[k] * coeff[j++]
for (k = taps - 1; k > idx;  k--)  acc += history[k] * coeff[j++]
y = acc >> 15
```

### It is a delay-line discriminator

The next step takes `history[idx - state[0x06]]` — the input delayed by
`state[0x06]` samples, which `B103FP_create` sets to **4**. Multiplying the
current sample by a delayed one is the classic FSK discriminator: the product's
DC component varies with the input frequency, so a lowpass afterwards yields a
signal whose sign is the received bit.

At the demodulator's 2400 Hz (finding 24), a 4-sample delay is one sixth of a
cycle at 900 Hz — sitting between the Bell 103 originate tones of 1070 and
1270 Hz in a way that makes the discriminator monotonic across the pair. That
is consistent but **not verified**; it should be checked against the actual
tone pairs before being relied on.

`state[0x08]`/`state[0x0c]` — the IIR lowpass, run through `FPM_iir_filt` —
are what smooth the product.

### Three points settled since

**The signature's hole is filled.** From the call site at `.text 0x08fc7b`:

```c
short FPM_FSD_demodulate(struct fpm_fsd *fsd, const short *samples,
                         unsigned short *bits_out, unsigned short count);
```

`arg2` is the **output bit buffer**, passed straight through from
`DemodDataB103`'s own third argument.

**The delay is 4**, read off a live object rather than out of
`B103FP_create` (finding 31) -- which matters, because `B103FP_create` is the
function this project has misread twice.

**The delayed read wraps properly.** `.text 0x0a7b45`:

```
sub  %edi,%eax          idx - delay
cwtl
test %ax,%ax
js   a7c9a       ->     add taps; rejoin
```

so a negative index has `taps` added rather than indexing before the buffer.
Worth checking explicitly: had it not wrapped, both objects would read the
heap bytes below their own allocation, those differ between allocations, and
the differential test would have agreed on most runs and not others -- the
same shape as the `0x2e`/`0xfe` bug in finding 30.

The discriminator itself then reads:

```
product = (short)((fir_out * history[idx - delay]) >> 13)
smoothed = FPM_iir_filt(product, iir_coeff, iir_state, iir_sections)
```

-- a shift of 13, not 15, so the product carries a gain of four into the
lowpass.

**Still to decode:** everything after the lowpass -- the slicer and the
bit-timing recovery that turns 8 samples per symbol into one bit. That is the
part where a subtle error shows up as a bit-error rate rather than a crash,
and where finding 31's warning about both tones giving a positive
discriminator output has to be resolved.

## 28. `FPM_AGC` — complete

`fpm_agc.c` is reconstructed and bit-exact: `src/dsp/fpm_agc.c`,
`include/dsplib/fpm_agc.h`, tested by `test/unit/t_fpm_agc.c` (50,200
differential checks). This supersedes the partial readings in finding 22.

```sh
objdump -d -r --start-address=0x0a66c0 --stop-address=0x0a6a20 \
        ../slmodemd/dsplibs.o
```

### It is a block AGC and a noise gate at once

`count` samples are chopped into blocks of `cfg.block_len` (36 for Bell 103,
which is exactly what `FPM_rms`'s 1/36 scaling was dimensioned for). One RMS
per block decides that block's fate:

```
level = FPM_rms(block)

silence  if (level < acquire_level && mult == 0)      /* nothing acquired yet */
      or if (level < squelch_level && mult != 0)      /* running, but too quiet */
adjust   otherwise
```

Silenced blocks are **zeroed**, not attenuated. Bell 103's thresholds are 10
and 80 against a full-scale RMS of ~23,000, so both are near-silence: the
first is "is there anything at all", the second a proper noise-gate floor once
the loop is running.

### The gain, and where the factor of two lives

```
recip ~= 2^30 / (level * 2^norm)      from FPM_div
mult   = (recip * ref_level) >> 15
y      = ((x * mult) >> 15) << (norm - 1)
```

which collapses to `y = x * ref_level / (2 * level)`. **The loop settles at an
output RMS of `ref_level / 2`** — 8192 for Bell 103, a quarter of full scale,
not the 16384 the config field's value suggests. The factor of two is the
`norm - 1`, which reads like an off-by-one and is not. Check it with numbers:
`level = 8192` gives `norm = 2`, `recip = 32768`, `mult = 16384`, `shift = 1`,
i.e. a gain of exactly 1.0 — which is what "settled" should mean at 8192.

### The block partition has two sharp edges

```
blocks = count / block_len;  tail = count % block_len;
if (block_len/2 <= tail)  blocks++;        /* the tail is its own block */
else                      tail += block_len;   /* fold it into the last one */
```

- **`count` below `block_len/2` is passed through completely untouched** —
  not gated, not scaled. For Bell 103 that is any call of 1..17 samples.
  `signal` is still written (as 0). `t_fpm_agc` sweeps every length 0..200.
- the folded block can reach `block_len + block_len/2 - 1` = **53** samples,
  which is past what `FPM_rms` is dimensioned for. That does not wrap: see
  finding 29.

### Corrections to finding 22

| finding 22 said | actually |
|---|---|
| `Release` sets `state[0x28] = 0` | it also clears `state[0x20]`, the level estimate — but **not** the gain, so the squelch stays at the higher threshold |
| `state[0x20]` is "gain state" | it is the smoothed **level** estimate; the gain is `mult`/`shift` at `+0x24`/`+0x26` |
| `state[0x0c]`/`[0x10]` are "a decay factor" and "a term scaled by level" | correct, and they are named in the blob: `AGC_DEF_ALPHA` and `AGC_DEF_BETA` |

### `state[+0x06]`, `[+0x08]`, `[+0x14]` and `[+0x18]`

Copied by `init` (`+0x18` is written by it), read by **no** function in
`fpm_agc.c`. Something upstream must consume them; resolve when
`DemodDataB103` is decoded. Left unnamed in the header per the project's
convention.

## 29. `FPM_sqrt_dp` saturates at 32703 — which is load-bearing

`FPM_sqrt_dp` clamps its table index (unlike `FPM_sqrt`, see D1), so its
return never exceeds **32703** for any 32-bit input:

```sh
# in claude_re/, against the reconstruction
printf '#include <stdio.h>\n#include "dsplib/fpm.h"\nint main(void){unsigned i,m=0;\
for(i=0;i<0xffffffffu;i+=65537u){unsigned r=FPM_sqrt_dp(i);if(r>m)m=r;}\
printf("%%u\\n",m);return 0;}\n' > /tmp/m.c
gcc -m32 -Iinclude -O2 -o /tmp/m /tmp/m.c src/dsp/fpm_sqrt.c -lm && /tmp/m
# 32703
```

Three consequences worth having written down:

1. **`FPM_rms` can never return a negative value.** Its result is
   `(short)FPM_sqrt_dp(...)`, and 32703 < 32768. This holds even when the
   accumulator overflows (D2) — a wrapped sum is handed over as a huge
   unsigned and saturates like any other.

2. **So `FPM_AGC_agc`'s `shift` is never negative** for any shipped
   configuration: `level` stays in [0, 32703], the smoother keeps
   `level_est` there because `alpha[0] + beta[0] == 32768` exactly, and
   `FPM_div` therefore always sees a denominator below 0x8000 and returns
   `norm >= 1`. The five-bit shift mask in the apply loop is dead code —
   reproduced anyway, because only the *second* of those conditions is a
   property of the data rather than the code, and D6 shows the data has a
   pair that breaks it.

3. **A very loud long block under-estimates its own level**, silently. At 53
   samples the true RMS passes 32767 somewhere around 82% of full scale and
   the reported level sticks at 32703, so the AGC applies less gain reduction
   than it should. Wrong, but in the safe direction, and only for blocks that
   the folding rule made over-long in the first place.

### An asymmetry in `FPM_rms` worth knowing when writing tests

`(x * 910) >> 15` is **0** for `0 <= x <= 36` but **-1** for `-36 <= x < 0`,
because the shift is arithmetic. So a small DC-free signal measures a few
counts rather than zero — 72 samples of ±4 measure 12, not 0. Picking a test
amplitude by computing the textbook RMS therefore misses the gate you were
aiming at; `t_fpm_agc` picks amplitudes against what `FPM_rms` actually
returns, and says so.

## 30. `FPM_TONE_detect` — the detector half, and what it says about D6

`FPM_TONE_detect` (`.text 0x0aaf80`, 488 bytes) is reconstructed and
bit-exact. It closes out `fpm_tone.c` apart from `FPM_TONE_find_rev` and
`FPM_TONE_kill`.

The structure is `FPM_MTD_detect`'s, one tone instead of a bank. Per sample:

```
hist[++idx] = x                       circular, `taps` words
sample  = (sum(hist[idx-j] * kernel[j]) for j in 0..taps-1) >> 15
energy  = (sample * sample) >> 15                      total
filtered = one section of FPM_iir_filt_II              the Goertzel resonator
in_band = (filtered * filtered) >> 15
excess  = energy - in_band                             out-of-band
```

and both `energy` and `excess` go through a first-order smoother before the
verdict. This is what `FPM_TONE_create` was building all along: the five words
at `+0x36` are `FPM_iir_filt_II` coefficients and the four at `+0x40` are its
direct-form-I state, which is why they are 5 and 4 rather than 5 and 2.

Reading `create`'s setup through `FPM_iir_filt_II`'s `{ b0, b2, b1, a2, a1 }`
ordering, the resonator is exactly the textbook damped Goertzel:

| slot | `create` writes | as a biquad |
|---|---|---|
| `+0x36` | `0x4000` | b0 = 1 |
| `+0x38` | `0x4000` | b2 = 1 |
| `+0x3a` | `-2 * cos` | b1 = −2cos ω |
| `+0x3c` | `(damp * damp) >> 16` | a2 = r² |
| `+0x3e` | `-(cos * damp) >> 14` | a1 = −2r cos ω |

so `damp` is the pole radius r in Q15 — the "0.96 resonator" of finding 18,
now pinned to a coefficient rather than inferred.

### The smoother coefficients corroborate D6

The two energy smoothers are hard-coded:

```
out_of_band = (31130 * out_of_band + 1638 * excess) >> 15
total       = (31130 * total       + 1638 * energy) >> 15
```

31130 + 1638 = 32768 exactly, i.e. unity DC gain. **31130 is precisely the
alpha that `AGC_DEF_ALPHA`'s slow pair should carry alongside its beta of
1638, and does not** — D6 predicted 31130 from `32768 - beta` before this
function was decoded, and here it is, written out longhand by whoever wrote
the tone detector. That is independent evidence that D6 is a copy-paste slip
and not an intentional design choice.

### Verdict

Same three values as `FPM_MTD_detect`, and the same meanings:

```
total < state[+0x0a]                       -> 2   no signal
out_of_band <= (ratio * total) >> 15       -> 1   tone present
otherwise                                  -> 0   signal, but not this tone
```

Negative `out_of_band` is clamped to zero on the way into the state — the
original does it branchlessly as `(~v >> 15) & v` — but only there, so it
stays negative for the remainder of a call.

### A test-methodology note

The first version of this test agreed with the blob on about eleven runs in
twelve. The failures were at object offsets `0x2e` and `0xfe`: the *high*
halves of two heap pointers, where the comparison skipped only the low halves.
Whether the two `malloc` results shared their top 16 bits decided the run.

Two things follow, both applied:

- when skipping a pointer slot in a byte-wise state comparison, skip **all
  four** bytes. The existing `compare_created` already did; the new one did
  not, and copying the wrong neighbour was enough.
- an intermittently-passing differential test is worse than a failing one,
  because eleven green runs read as evidence. `t_fpm_tone` is now run 40 times
  in a row when it changes.

## 31. Bell 103's configuration, read off a live object

Rather than infer these from `B103FP_create`'s 2151 bytes, build one with the
reference implementation and read the fields. Every number below is measured,
not derived:

```sh
# see the probe in the session log; links against build/dsplibs_ref.o
FSD cfg: fir_taps=15 delay=4 iir_len=3
         f0e=10 f10=1 f12=8 f14=6 f16=160 f18=0
FSD state: f22=4
FSM:     freq=1070/1270  sps=24  scale=3200  scaled=1188/1411
RX MRF:  L=3 M=10 taps=90 hist_len=30
AGC:     ref=16384 acq=10 sq=80 blk=36
```

What each confirms:

| observed | confirms |
|---|---|
| `freq = 1070 / 1270` | Bell 103 **originate** mark/space, exactly the standard |
| `sps = 24` | 300 baud at 7200 Hz — finding 17 |
| `scaled = 1188 / 1411` | freq × 10/9, so FPM_TONE's hard-wired 8 kHz yields 7200 |
| RX MRF `L=3 M=10` | 8000 × 3/10 = 2400 Hz — finding 24, measured |
| AGC = 16384/10/80/36 | `AGCb103_CFG` verbatim, so `b103_agc_cfg.c` is what `create` installs |
| FSD `delay = 4` | finding 27's claim, from the object rather than the disassembly |
| FSD `f12 = 8`, state `f22 = 4` | 2400 Hz / 300 baud = 8 samples per symbol, and half of it — bit timing |

### The 4-sample delay, worked through

Worth doing carefully, because the receive path **aliases** and the obvious
calculation is wrong. After the 3:10 decimation the sample rate is 2400 Hz, so
Nyquist is 1200 — and the space tone at 1270 Hz is above it. It folds to
2400 − 1270 = **1130 Hz**. The demodulator therefore sees 1070 and 1130, not
1070 and 1270.

A delay-line discriminator's DC output is cos(2π f D / fs) / 2 for D = 4:

| tone | as seen at 2400 Hz | 4ω | DC term |
|---|---|--:|--:|
| mark 1070 | 1070 | 1.783 cycles | +0.208 |
| space 1270 | **1130** (aliased) | 1.883 cycles | +0.743 |

So the two are well separated — but **both positive**. The slice threshold
cannot be zero, which is presumably what the three-section IIR and whatever
follows it are for. Finding 27 called the frequency plan "consistent but not
verified"; it is now verified, and the part that needed verifying turned out
to be exactly the part that a Nyquist-ignoring reading gets wrong.

This is the number to check first if the demodulator ever produces a stuck
output: a discriminator whose two states are both the same sign is one
mis-sited threshold away from never toggling.

## 32. `FPM_FSD_demodulate` — complete, and why loopback still carries no data

`.text 0x0a79f0`, 745 bytes, reconstructed and bit-exact:
`src/dsp/fpm_fsd.c`, 130,447 differential checks driven by **real** Bell 103
FSK from `ModDataB103` through the actual receive front end.

### The chain

Per input sample: 15-tap circular FIR, then the delay-line discriminator
(`>> 13`, so a gain of four), then `FPM_iir_filt` with three sections, then a
**Schmitt slicer** — the bit changes only when the lowpass output passes
±`slice_level` (10) and holds in between.

### The bit clock resynchronises on edges

Two counters, and this is the part worth reading twice:

| | |
|---|---|
| `since_bit` | samples since a bit was last emitted |
| `disagreements` | samples on which the slicer disagreed with the committed bit — **cumulative within the bit period, not consecutive**, and reset only when a bit is emitted |

A bit comes out either when the slicer agrees and `bit_samples` have elapsed
(free-running, carrying a run of identical bits), or when `disagreements`
reaches `bit_samples / 2`, at which point the new bit is committed, emitted,
and **both counters reset** — which drags the bit clock back into step with
the sender. There is no separate phase detector; the edge simply restarts the
count. A glitch shorter than half a bit never reaches the output.

`bit_samples == 5` is special-cased to a half of 3 rather than 2; every other
value is shifted right.

### Output cap: input is discarded, not held over

The loop stops once `max_bits + 1` bits have been written and **throws away
the remaining samples**. Bell 103's `max_bits` is 6 and `bit_samples` is 8, so
the ceiling is 8 bits from 64 samples. `DemodDataB103` feeds 48 at a time, so
it never bites in normal use — but a caller passing a longer fragment loses
the tail silently.

### The receiver's actual passband, measured

**Corrected by finding 33** -- this is the response *after* the mixer, not the
radio-frequency passband. Read the figures as baseband. Driving tones through
`rx_mrf -> AGC -> FSD` and reading the slicer input out of the trace buffer:

| input | slicer level (no AGC) |
|--:|--:|
| 600 Hz | −3106 |
| 750 Hz | −1234 |
| **775 Hz** | **~0 — the discriminator null** |
| 900 Hz | +3876 |
| 1100 Hz | +331 |
| 1250 Hz | −10 |
| ≥1300 Hz | ~0, out of band |

So the usable range is roughly 600–1200 Hz with the crossing at 775. With the
AGC supplying gain, 1070 slices to 1 and 1270 to 0 — the pair **is** resolved,
confirmed directly:

```
Bell103 originate 1070/1270   bit(1070)=1  bit(1270)=0   RESOLVED
Bell103 answer    2025/2225   both 0        -- cannot carry data
V.21 channel 1     980/1180   both 1        -- cannot carry data
```

### Why a loopback measures BER 0.485

Feeding `ModDataB103`'s output straight back in gives essentially random bits.
This is the **blob** doing it, not the reconstruction — the two agree
bit-for-bit throughout — and the cause is configuration, not DSP:

1. A Bell 103 originate station transmits 1070/1270 and receives 2025/2225.
   Looping its own transmitter back is not a valid link in the first place.

2. 1270 Hz sits on the receive filter's skirt, ~20 dB below 1070. With a
   steady tone the AGC winds up enough to slice it; in a data stream the AGC
   gain is set by the strong 1070 bursts, so the 1270 bursts never reach
   −`slice_level` and the slicer holds. The output is ~2/3 ones.

3. **`B103_CFG` does not build a link-capable object.** `B103FP_create` picks
   between `B103_BPF_CALLER` (40 taps) and `B103_BPF_ANSWER` (50 taps) on
   `b103fp` state `+0x04`, and stores the choice at `dsp[+0xf4]` with its
   length at `+0xfa`. Built from `B103_CFG` those fields come out **NULL and
   zero** — the branch is never reached. The tone pairs are chosen in the same
   region: 1070/1270 (Bell 103 originate), 2025/2225 (answer), 980/1180
   (V.21 channel 1).

So the per-direction bandpass is selected by a configuration `b103_create`
builds from its `caller` argument, and `B103_CFG` alone is a template. Nothing
reconstructed so far applies the filter at `dsp[+0xf4]`; finding its consumer
is part of the `B103FP_modem` / `DemodDataB103` work.

**This does not block anything already done.** Every module on the path is
bit-exact against the blob on exactly these objects. It is a statement about
what still has to be reconstructed before the datapump can complete a call,
and it is the answer to "why does loopback not work" — asked and answered
before it could be mistaken for a DSP bug.

## 33. `DemodDataB103` — the receiver mixes to baseband, and the tone verdict was backwards

Two corrections to earlier findings, both from measuring rather than reading.

### The receiver is a superheterodyne

`DemodDataB103` multiplies the incoming samples by a locally generated tone
**before** anything else touches them:

```c
FPM_TONE_generate_demod(fp->hdx->tone_lo, lo, count);
for (i = 0; i < count; i++)
        in[i] = (short)((in[i] * lo[i]) >> 14);
```

Read off a live object, that oscillator runs at **1350.1 Hz** (phase increment
5530 at 8 kHz). So the Bell 103 answer channel comes down as:

| tone | mixed with 1350 | slicer level |
|--:|--:|---|
| 2025 Hz | **675 Hz** | negative |
| 2225 Hz | **875 Hz** | positive |

and the discriminator null measured in finding 32 sits at **775 Hz** —
squarely between them. The frequency plan is exact, and it was predicted from
the null before the oscillator was found.

This corrects finding 32's passband table: those figures are the *baseband*
response of everything after the mixer. Feeding 1070/1270 straight into
`rx_mrf` bypasses the mixer entirely, which is why they appeared to be the
resolved pair. They are not; the caller receives 2025/2225, as Bell 103 says
it should.

### `FPM_TONE_detect`'s verdicts are the reverse of what finding 30 recorded

The biquad `FPM_TONE_create` builds at `+0x36` is `{ 1, -2cos w, 1 }` over a
pole pair at radius r — a pair of zeros **on** the unit circle, i.e. a
**notch** at the tone frequency, with the poles only narrowing it. So what the
state calls out-of-band energy is the signal with the tone *removed*, and the
quantity compared against `ratio` is the tone's own share.

Measured on a live detector, 12000-amplitude sine in:

| input | E_total | tone share | verdict |
|--:|--:|--:|--:|
| 1900 Hz | 225 | 2 | 1 |
| 2000 Hz | 1175 | 363 | 1 |
| **2100 Hz** | 1810 | **1810** | **0** |
| 2200 Hz | 1179 | 367 | 1 |
| 2300 Hz | 224 | 0 | 1 |
| below 1900 / above 2300 | 0 | 0 | 2 |

So **0 means the tone IS present**. Corrected in `include/dsplib/fpm_tone.h`;
the constant formerly called `FPM_TONE_ABSENT` is now `FPM_TONE_PRESENT` and
vice versa. `FPM_MTD_detect`'s constants were named by the same analogy and
have **not** been re-checked — flagged in the header rather than assumed.

### Which makes the acquisition sequence read correctly

`DemodDataB103` advances `rx_state` by five on each `FPM_TONE_PRESENT`. With
the polarity right, that is a **calling modem listening for the 2100 Hz answer
tone**, exactly as V.25 specifies — not, as the first reading had it, listening
for energy that is anything but. Three consecutive detections reach 15, which
freezes the data AGC (sensible: the gain is locked on a steady tone rather than
on data) and switches to demodulating at 16, after which the detector is never
consulted again.

### Two AGCs

`dsp->agc` at `+0x0c` (36-sample blocks) serves the data path, after the mixer
and rate conversion. `dsp->det_agc` at `+0x38` (40-sample blocks) runs on an
untouched **copy** of the input, before the mixer, and feeds only the tone
detector. That separation is what lets the data AGC be frozen at acquisition
without blinding the detector. It also completes the DSP block: `+0x38..+0x63`
was the last unattributed region, and a second `struct fpm_agc` fits it
exactly.

## 34. The Bell 103 call-setup state machine, with the author's own names

Ten functions reconstructed and bit-exact: seven half-duplex states and the
three tables that sequence them. 72,114 differential checks.

### Shape

`hdx` carries **two** current states, not one — transmit at `+0x08` and
receive at `+0x10` — and a substate number at `+0x14`. Every Hdx function has
the same signature and the same shape:

```
do this state's work
zero the caller's count
move a counter
if the counter says so, call B103NextState[hdx->mode](fp)
```

No Hdx function chooses its own successor. That is entirely the business of
the three tables, which is why the same seven states serve originate, answer
and loopback. `B103NextState` is a three-entry function-pointer table at
`.data:0x77e8`: **loopback, originate, answer**, in that order.

### The substate names are the original's

The blob still carries the debug strings, so these are not invented:

```sh
python3 - <<'EOF'
import subprocess
d = subprocess.run(['objcopy','-O','binary','--only-section=.rodata.str1.1',
                    'dsplibs.o','/dev/stdout'], capture_output=True).stdout
for off in (0x3d8b, 0x3d5c, 0x3d70, 0x3d9d):
    print(repr(d[off:d.index(b'\0', off)].decode()))
EOF
# 'B103_STATE_START\n' 'B103_STATE_CARRDET\n' 'B103_STATE_WAIT1\n' 'B103_STATE_WAIT2\n'
```

| substate | name | originate does |
|--:|---|---|
| 0 | `B103_STATE_START` | transmit silence, listen for the answer tone, `tone_timeout` blocks |
| 1 | `B103_STATE_CARRDET` | tone heard — hold eight more blocks |
| 2 | `B103_STATE_WAIT1` | transmit 40 blocks of mark |
| 3 | `B103_STATE_WAIT2` | data both ways |
| 4 | *(none)* | connected; asking again prints "default" |

Answer and loopback have no WAIT2 and jump from WAIT1 straight to 4. The
answerer transmits mark from START — that mark **is** the tone the caller is
listening for.

### The handshake is asymmetric, and deliberately

`TxHdxMarksB103` exits on different conditions in the two directions:

```c
if (fp->is_answer) {
        if (hdx->tx_blocks <= 0) advance;
} else if (hdx->tx_blocks <= 0 && fp->dsp->rx_state > 14) {
        advance;
}
```

An answering modem stops after its block count regardless. A **calling** modem
also waits for its own receiver to have acquired. So the caller holds mark
until it hears the answering modem, which is what Bell 103 call setup
requires and is not something either side could do alone.

### `RxHdxStartB103` is installed by nothing

None of the three tables ever selects it — they go straight from
`RxDetMarkB103` to `RxHdxDataB103`. Either `B103FP_create` uses it as the
initial receive state or it is dead. It is reconstructed and driven directly
by the test rather than left uncovered; resolve which when `B103FP_create` is
decoded.

### Status and flags are B103's own, not `DPSTAT_*`

The tables set `fp->status` to 2, 3, 4 and 7, and the Hdx states set 5 and 6
on their failure paths. These are **not** the `DPSTAT_*` codes in `dp.h` — 7
here means connected, where `DPSTAT_BUSY` is 7. Something above this layer
maps them; `B103FP_modem` is the candidate. Left as literals rather than given
invented names.

### Testing control flow differentially

The state is a function pointer, so it necessarily holds a different value in
the reconstruction than in the blob. Comparing pointers fails on every
transition; comparing nothing tests nothing. `t_b103hdx.c` maps each pointer
through a table of the seven known states and compares the resulting index.

One subtlety cost a run: **both** objects are built by the reference
`B103FP_create`, so both start holding reference pointers, and only the slots
a table has since overwritten differ. The lookup therefore has to search both
tables — searching only the reconstruction's reported every untouched slot as
unknown.

## 35. Config word 0 is the call type — and the link works, at BER 0

The open question from finding 32 — how does one get a link-capable
configuration — has a one-word answer. `B103FP_create`'s config word 0 is the
**call type**, and `B103_CFG` sets it to 2, which is loopback.

Found by sweeping rather than by reading the 2151 bytes: build an object for
each value of each config word and print what came out.

| word 0 | | `hdx->mode` | bandpass | tone detector | transmits | local oscillator |
|--:|---|--:|---|---|---|--:|
| 0 | **originate** | 1 | `B103_BPF_CALLER`, 40 taps | yes | 1070 / 1270 | **1350.1 Hz** |
| 1 | **answer** | 2 | `B103_BPF_ANSWER`, 50 taps | yes | 2025 / 2225 | **395.0 Hz** |
| 2 | **loopback** | 0 | none | no | 1070 / 1270 | 1350.1 Hz |

### The frequency plan, closed

Each side mixes the pair it *receives* down to the same place:

| side | receives | LO | at baseband |
|---|---|--:|---|
| originate | 2025 / 2225 | 1350 | **675 / 875** |
| answer | 1070 / 1270 | 395 | **675 / 875** |

and 675 and 875 straddle the 775 Hz discriminator null measured in finding 32.
One demodulator design, one filter set, both directions — the oscillator is
the only thing that differs. The null predicted 1350 before the oscillator was
found; the answer side's 395 then fell out of the same arithmetic.

### It carries data

`test/unit/t_b103link.c` runs an originating transmitter into an answering
receiver over a noiseless channel, 4000 bits of a maximal-length sequence:

```
  blob -> blob   3996 bits sent, 3991 received, lag -3, BER 0.00000
  ours -> ours   3996 bits sent, 3991 received, lag -3, BER 0.00000
  ours -> blob   3996 bits sent, 3991 received, lag -3, BER 0.00000
  blob -> ours   3996 bits sent, 3991 received, lag -3, BER 0.00000
```

Zero errors, not "low" — the channel is ideal, so one error would be a defect
rather than bad luck. The mixed pair matters and the test asserts it: feeding
a station its own transmitter measures **BER 0.485**, because a station does
not receive the band it transmits.

Bit-exactness already implied the cross combinations would work, so they are
not new evidence. They are the form the claim has to take to mean anything to
someone deciding whether to point this at real hardware.

### Two corrections

- **`fp[+0x04]` is not the caller/answer selector.** Finding 32 read the
  branch at `.text 0x08ecd7` as switching on it. It does — but that whole
  region is gated by an earlier test on word 0, and changing `+0x04` alone
  changes nothing observable. Sweeping caught this in a minute; reading would
  not have.
- **The bandpass history is usually zero.** D7 stands — two objects from the
  same `create` really did filter the same input differently — but a *fresh*
  allocation comes back zeroed, so the exposure needs a dirtied heap. D7 is
  re-classified as an out-of-contract divergence with the boundary stated,
  rather than claiming a faithfulness that is not available.

## 36. `B103FP_create` — the common path, mapped

Working notes for the 2151-byte constructor. The three call-type branches and
the four allocation paths are not yet decoded; **everything below is the path
all three call types share**, and it is recorded now so the mapping is not
lost between sessions.

### Shape

It is not 2151 bytes of logic. It is a sequence of *build a config on the
stack from the library default, patch the Bell 103 fields into it, call the
init*, repeated eight times. The relocation census makes that plain:

```sh
objdump -d -r --start-address=0x8e690 --stop-address=0x8ef00 dsplibs.o \
  | grep R_386 | awk '{print $3}' | sort | uniq -c | sort -rn
```

```
9 FPM_TONE_CFG   7 FPM_FSD_CFG   7 B103_CFG   6 sysdep_malloc
4 FPM_TONE_create   3 FPM_MRF_CFG   2 FPM_MTD_CFG   2 FPM_MRF_init
2 FPM_FSM_CFG   2 FPM_AGC_init   2 AGCb103_CFG   ...
```

### Order of construction

| # | what | notes |
|--:|---|---|
| 1 | copy 28 bytes of config into `state[0x00..0x1b]` | seven dwords |
| 2 | `hdx->tone_timeout = max(cfg.tone_timeout_ticks / 20, 700)` | see the sign note below |
| 3 | `hdx->tx = TxHdxStartB103`; `tx_blocks`, `rx_count`, `substate`, `r18`, `tone_detect`, `tone_lo` all zeroed | |
| 4 | `hdx->tone_lo = FPM_TONE_create(NULL, patched FPM_TONE_CFG)` | then `inc = 0x159a` (1350.1 Hz), `phase = 0` |
| 5 | **branch on `call_type`** | 0 originate, 1 answer, else loopback |
| 6 | `FPM_MRF_init(&dsp->tx_mrf, ...)` | `FPM_MRF_CFG` patched: 10:9, `B103_MRF_FILT_TX`, 270 taps |
| 7 | `FPM_MRF_init(&dsp->rx_mrf, ...)` | patched: 3:10, `B103_MRF_FILT_RX`, 90 taps |
| 8 | `FPM_AGC_init(&dsp->agc, AGCb103_CFG, 0)` | |
| 9 | `FPM_AGC_init(&dsp->det_agc, AGCb103_CFG, 0)` | then `dsp[0x42] = 40` — the acquisition AGC's block length is **patched after init**, which is why the two AGCs differ (36 vs 40) despite sharing a config |
| 10 | `FPM_FSM_init(&dsp->fsm, ...)` | tones per branch, 24 samples/symbol, scale from `cfg.tx_scale` |
| 11 | `FPM_FSD_init(&dsp->fsd, ...)` | `B103_CHAN_INTRP`, delay 4, `B103_IIR_LPF` |
| 12 | `dsp[0xe4] = FPM_MTD_create(dsp[0xe4], ...)` | `MTDb103_COEF`, 2 tones, ratio `0x3666`, min level 2 |

So `dsp[+0xe4]` — previously an unnamed reserved word — is the **multi-tone
detector object pointer**, and the DSP block is now completely accounted for.

### Fields the tail sets

```
dsp->r00      = 1        (the value DemodDataB103 copies into agc.f18)
dsp->rx_energy = 0 ; dsp->rx_tone = 0 ; dsp->rx_state = 0
state[0x20]   = dsp->fsd.trace       (dsp + 0xb8)
state[0x28]   = &dsp->fsd.last_count (dsp + 0xbc)
state[0x1c]   = 0, then flags |= 0x40, then status = 1
state[0x24], [0x2c], [0x30], [0x34], [0x38], [0x3c], [0x40],
state[0x44], [0x48], [0x4c] = 0
```

`status = 1` and `flags = 0x40` on exit match what the probes read back.

### `call_type` out of range sets an error and builds a loopback anyway

```
cmp $0x1,%eax ; je  answer
              ; jb  originate
cmp $0x2,%eax ; je  loopback
              ; else: flags |= 0x02 ; status = 5 ; fall through to loopback
```

So `call_type` of 3 or more is flagged (status 5 is "Bell103 internal error
detected!", finding 34) but still produces a working loopback object. That
matches the sweep, which showed mode 0 for every value tried above 2.

### The timeout clamp is unsigned, and the divide is signed

```
t = cfg.tone_timeout_ticks / 20     /* signed, via the 0x66666667 idiom */
cmp $0x2bc,%edx
jae  keep                            /* UNSIGNED compare */
t = 700
```

A **negative** `tone_timeout_ticks` divides to a negative `t`, which as
unsigned is enormous, passes the `jae`, and is stored as a negative short.
The clamp is meant to enforce a floor and does not, for that one class of
input. Not reachable from `B103_CFG` (14000/20 is exactly 700, so the clamp
is a no-op there) and not yet reachable from anything else, so it is recorded
here rather than as a deviation until `b103_create` is decoded and the caller
of this field is known.

### Still to decode

The three call-type branch bodies (which install the bandpass, the tone
detector, the transmit tones and the oscillator — all four already known by
measurement, finding 35), the `loop_high_channel` branch, and the four
allocation paths: object (0x58), `hdx` (0x24), `dsp` (0x100), and the
default-config path taken when `cfg` is NULL.
