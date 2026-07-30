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

## 22. `FPM_AGC` — structure (agc() reconstruction pending)

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

**Still to decode:** the gain update itself — the ~450 bytes after the
threshold comparison, including how `FPM_div` is used to form the gain and
where the freeze flag short-circuits. That is the part worth getting right,
since it determines the loop's time constants.

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
