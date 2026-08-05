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

## 37. `MEMORYC.c` — not identifiable, and not a phase 1 remainder

Carried since finding 1 as "the one TU with no function attributed to it".
Investigated properly; the answer is that it cannot be identified yet, and the
reason is structural rather than a gap in the tooling.

### Where it sits

`MEMORYC.c` is one of **fourteen** translation units sharing a single
unresolvable address bracket, `.text 0x05dd10-0x07a9f0`:

```
V34hshak.c  v34filters.c  detector.c  DFTC.c  DPSK.c  MEMORYC.c
V8Interface.c  V8global.c  V8.c  V8Detector.c  V8Dftc.c  V8Dpsk.c
V8Fsk.c  Callprog.c
```

That is the V.8 negotiation and V.34 handshake cluster. The bracket holds 113
functions and none of the fourteen contributes a local symbol, so there is
nothing to split it on: `tuattrib.py` assigns what it can by name prefix and
puts the remainder in the nearest TU as `fill`. 95 of the 113 are `fill` or
`name-only`.

### It is not an allocator

The name invites the reading "memory management", and that is checkable:

- **No memory-flavoured symbol anywhere in the bracket.** Searching all 113
  for `mem|alloc|free|buf|pool|heap` returns exactly one hit, `v34FreezeEcho`,
  which is a false positive on "free".
- **No arena.** The largest `.bss` object in the whole library is 320 bytes
  (`rx_in_internal`, a staging buffer). A pooled allocator would need a static
  block and there is none.
- **Everything allocates through `sysdep_malloc`.** Every `*_create` in the
  reconstruction so far calls it directly; nothing routes through a library
  allocator.

So whatever `MEMORYC.c` holds, it is not a general allocator, and the guess
that it might be is now ruled out rather than left hanging.

### Re-scoped

Identifying it requires attributing that bracket, and attributing that bracket
requires reconstructing V.8 — the functions have to be recognised by what they
do, since their names will not do it. It was listed as a **phase 1 remainder**
and it is not one: it belongs to phase 5.

Nothing depends on it. No reconstructed function calls into the bracket, and
Bell 103 is complete without it.

## 38. The `b103fp.flags` bits — six of eight are write-only

Resolved by enumerating every access rather than by guessing meanings.

```sh
# every read and write of the status word in the b103fp translation unit
objdump -d ../slmodemd/dsplibs.o \
        --start-address=0x8e690 --stop-address=0x8fd00 \
  | grep -E '0x1[cd]\(%e'
```

Sorting those by whether they load or store gives the answer directly. There
are exactly **two** loads of `+0x1d` in the whole library:

- one in `B103FP_create`, a read-modify-write (`flags = (flags & ~0x40) | 0x34`)
  that tests nothing;
- one in `B103FP_modem`, which clears `0x02` and then tests `0x01`.

So `0x01` is the only bit whose value ever changes behaviour. `0x02` is never
tested but *is* consumed — cleared at the top of every `B103FP_modem` call —
which makes it a one-shot event a caller must read each block or lose.

**The remaining six are written and never read.** And they cannot be read from
outside either: `b103_process` masks `B103FP_modem`'s return with `0xff`, so
the flags byte never leaves the library.

| bit | set by | tested? |
|---|---|---|
| `0x01` | Originate WAIT2 | **yes**, by `B103FP_modem` |
| `0x02` | every timeout path | no, but consumed |
| `0x04` | Originate/LocLoop/Answer WAIT1, `create` | no |
| `0x08` | Originate WAIT2, LocLoop/Answer WAIT1 | no |
| `0x10` | START, all three tables | no |
| `0x20` | `RxHdxData`, tracking carrier | no |
| `0x40` | `B103FP_create`; cleared at CARRDET | no |
| `0x80` | nothing sets it; `RxHdxData` clears it | no |

### Why they are still not named

They are reproduced faithfully — they cost nothing and a caller outside the
blob may want them. But they are left as literals. A name asserts a meaning,
and the meaning of a bit that nothing reads cannot be recovered from the code;
inventing one would put a claim in the header with nothing behind it.

`0x80` is the clearest case: **nothing in the library sets it**. Whatever it
was for, the code that set it is not here.

### Scope

`struct dp`'s `status` is not a bit set. It holds a `DPSTAT_*` scalar, assigned
0, 1 or 4 by `b103_process`. No other bit-set field has appeared so far;
re-check when V.22 and V.32 are reached.

## 39. The resampler filter audit — the LSB residual is quantisation, not design

`rcfilter.py` reports each bank's fit as a maximum coefficient error in LSB,
and that number has been quoted throughout this project as if it measured
something. It does not measure what it appears to. Audited with
`tools/filteraudit.py`; four questions, four measurements.

### 1. What six LSB actually costs

Mode 2's prototype, 217 taps, fitted at beta 7.25 / fc 4498 Hz, max error
6 LSB and only 20 of 217 coefficients exact. In the numbers a filter designer
reads:

| design | passband ripple | stopband floor |
|---|--:|--:|
| **the original** | 0.0125 dB | **−73.5 dB** |
| our fit, quantised to Q14 | 0.0030 dB | **−77.4 dB** |
| our fit, unquantised | 0.0029 dB | −84.3 dB |
| response-fitted, unquantised | 0.0174 dB | **−108.8 dB** |

Two things fall out.

**Our fit is already better than the original** — 3.9 dB more stopband
rejection and four times less passband ripple. The 6 LSB is not a deficiency
to be chased; it is the distance between two designs that both sit near the
Q14 floor.

**Quantisation, not design, is the binding constraint.** The response-fitted
design achieves −108.8 dB unquantised and −79.0 dB in Q14 — thirty decibels
lost to fourteen bits. Any effort spent making the *design* better than about
−80 dB is wasted unless the coefficient width changes with it.

Stopband figures here start at 1.35× the −6 dB point, the allowance
`rcfilter.measure` already used. Measuring from the binding Nyquist instead
puts the transition band in the window and reports about −20 dB for every
filter, original included — worth stating because the first version of this
audit did exactly that and the number looked alarming.

### 2. Rounding mode: ruled out

| mode | max err | exact |
|---|--:|--:|
| half-up | 6 | 20/217 |
| truncate | 6 | 16/217 |
| floor | 6 | 22/217 |
| half-even | 6 | 20/217 |

All four give the same maximum error. The library's other tables truncate
(the sine, sqrt and div tables all do), so truncation was the obvious
hypothesis; it is not the explanation. Stop looking here.

### 3. Per-branch normalisation: ruled out

Mode 2's six polyphase branches sum to `16384, 16382, 16383, 16384, 16383,
16382` — a spread of 2 about a mean of 16383. Had the designer normalised
each branch to unity DC gain they would all be 16384. They are not, so the
prototype was designed and quantised as one filter and the branches inherited
whatever the rounding gave. The whole-prototype normalisation the tool already
does is the right model.

### 4. Objective: it depends what the fit is for, and the tool should say

Minimising response error instead of coefficient error picks a genuinely
different design — beta 10.6 rather than 7.25 — that is 24 dB better
unquantised and 1.6 dB better in Q14, but sits 180 LSB from the original.

So the two objectives answer different questions:

- **Reproducing** the original — which is what `docs/coefficients.md` is for —
  wants coefficient distance. 180 LSB is not the same filter.
- **Regenerating** at another sample rate — which is the project's actual goal
  for phase 12 — wants response. There is no original to be near.

The tool currently does the first and is documented as if it did the second.
That is the one real defect this audit found, and it is a documentation and
interface problem rather than a numerical one.

### What changes

- `filteraudit.py` is added, and reports ripple and stopband alongside the LSB
  figure. An LSB count without the quantisation floor beside it is not
  interpretable.
- The "our design is ~9.6 dB better" note recorded earlier was measured
  loosely; the careful figure is **3.9 dB**, and it is the *quantised*
  comparison that matters.
- Phase 12 should regenerate with the response objective, not by fitting the
  existing coefficients and rescaling them.

## 40. SpanDSP interop — the transmitter is validated, the receiver has a level sensitivity

The first test in this tree that is not a comparison with the blob. Every
other one establishes equivalence, which proves nothing about correctness: if
the blob were wrong, a bit-exact reconstruction would be wrong identically and
every test would still pass. This talks to SpanDSP, written from the standard
by someone else.

```sh
make interop            # needs third_party/spandsp built -- see its README
```

**Version matters, and the first run of this got it wrong.** This was written
against the distro's `libspandsp-dev` 0.0.6 rather than the SpanDSP 3 the plan
called for, and 0.0.6 ships its two Bell 103 preset entries **swapped**: its
`FSK_BELL103CH1` is the answerer's 2025/2225 where 3.x's is the caller's
1070/1270. The substitution was convenient and went unflagged, and the
conclusion drawn from it -- "SpanDSP numbers these backwards from the obvious
reading" -- was a statement about one buggy release presented as a fact about
SpanDSP.

Re-run against **3.1.0** (`6a0e9f51`, built from source) with the correct
channel numbering, every result below is unchanged, including the 229-bit
figure exactly. The test now asserts both channels' frequencies before using
them, so a build against 0.0.6 fails immediately and says why.

### The transmitter passes outright

```
ours -> SpanDSP: 2994 bits sent, 2993 received, lag 0, BER 0.00000
```

SpanDSP's Bell 103 receiver recovers our originating transmitter's bit stream
with **zero errors**. Tone pair, baud rate, mark/space polarity and pulse
shaping are all confirmed against an independent implementation.

### The receiver loses lock after ~229 bits, at one input level

```
SpanDSP -> ours: clean for 229 bits, then lock is lost permanently
```

And the dependence is on **amplitude**, in a way that is not monotonic:

| gain applied to SpanDSP's samples | result |
|---|---|
| ×0.25, ×0.50, ×1.00, ×2.00, ×4.00 | loses lock after ~230 bits |
| ×0.75, ×1.25, ×1.50, ×1.75, ×2.25, ×2.50, ×2.75, ×3.00 | **BER 0 over 4000 bits** |

Every failing gain is an exact power of two — the cases where `sample * n / d`
is a pure shift and introduces no truncation. Every working gain is one that
does. But adding dither directly (±1 alternating, or a +1 DC offset) does
**not** fix it, so it is not simply that the receiver needs noise.

Ruled out along the way:

- **Not the slicer's dead zone.** The slicer input ranges over
  [−1936, +2091] in both the working and failing cases, with under 0.5% of
  samples inside the ±10 dead zone in either.
- **Not the tone plan.** The first 229 bits are demodulated perfectly, so the
  oscillator, bandpass and discriminator are all correct.
- **Not nondeterminism.** Identical across runs.

### RESOLVED: it is D4, firing in the AGC

Both questions below are now answered; the text is kept because the route to
the answer is the useful part.

**Attribution:** the blob does exactly the same thing. `gen_spandsp_capture`
freezes the signal and `t_spandsp_replay` runs it through both — 116,954
agreeing checks, and both lose lock at the same bit.

**FIXED.** D4's table entry is now correct by default, with
`-DDSPLIB_REPRODUCE_BUGS` restoring the original's zero for the differential
tier. Both directions of this test now measure **BER 0.00000**.

**Mechanism:** at the failing block the AGC's gain becomes **zero** and the
block is multiplied to silence. Its level estimate, 4088, normalises to
mantissa `0xff80` — the value that indexes one past `FPM_div`'s table, which
returns a zero reciprocal. That is **D4**, previously filed as a defect that
could fire and now shown to drop a call. It fires on 46 of the 700 blocks in
this capture.

**And the power-of-two pattern was the clue, not a red herring.** Scaling the
input by 2^k scales the level estimate by 2^k, which leaves the normalised
mantissa *identical* — so every power-of-two gain re-triggers the same
out-of-range index and every other gain escapes it. A failure that survives a
factor of eight in level but not a factor of 1.25 was never a level problem.

### What this is not evidence of

**It is not a reconstruction bug.** Every module on this path is bit-exact
with the blob over millions of checks, so the blob behaves the same way. What
is not yet established is whether it is a defect in the *original* or a
property of the signal SpanDSP produces — its transmitter is not obliged to
match Smart Link's pulse shaping, and a real Bell 103 peer might never produce
this waveform.

Attributing it needs a third measurement: capture SpanDSP's samples to a file
and replay them through blob and reconstruction side by side in the 32-bit
differential harness. If they diverge, it is ours; if they agree, it is the
original's, and then the question is whether it matters on a real line.

### How the test records it

`t_spandsp_b103.c` asserts the clean prefix is at least 150 bits rather than
marking the case "expected fail". An expected-fail marker rots; asserting the
behaviour actually observed means a change in **either** direction — fixed, or
degraded — fails the test.

### Why it is a separate binary

The system SpanDSP is amd64 and the blob is i386, so the two tiers cannot
share a build. That turned out to be a feature: `make test` answers "is it the
same as the blob" and `make interop` answers "is it a correct modem", and the
build makes the distinction visible rather than leaving it in prose.

Building 64-bit also found a real portability bug that `make check64` could
not: `FPM_TONE_create` allocated a hard-coded 264 bytes for a struct that is
larger when pointers are eight bytes. `check64` only compiles; this ran.

---

## 41. The call-progress path runs at 8000 Hz, whatever the host rate

`call_create` takes the host sample rate as its last argument and builds two
resamplers around the supervisor:

```
    rate == 8000    neither converter is created; CALLPROG sees the host
                    samples directly
    rate == 9600    +0x14 = RcFixed_Create(3)   9600 -> 8000  (down, 6:5)
                    +0x18 = RcFixed_Create(2)   8000 -> 9600  (up,   5:6)
    rate == 48000   +0x14 = RcFixed_Create(5)  48000 -> 8000  (down, 6:1)
                    +0x18 = RcFixed_Create(4)   8000 -> 48000 (up,   1:6)
```

Any other rate leaves both converters null and logs at debug level 2. So the
whole of Callprog.c, Cadence.c, DualTone_Detector.c and CallingTone.c is
written against a fixed 8000 Hz, and every frequency in their coefficient
tables should be read at that rate. `CALLPROG_Create`'s `imul $0x1f40` --
8000 -- confirms it independently: its timeouts are seconds times the sample
rate.

This matters more than it sounds. Read at 9600 the supervisor's band filter
looks like a 200 Hz to 1600 Hz bandpass with a notch at 2712 Hz, which is
plausible enough to go unquestioned. Read at 8000 it is 135 Hz to 1280 Hz with
the null at 2260 Hz -- and 350, 440, 480 and 620 Hz, every call-progress tone,
land inside 1.3 dB of each other. The 9600 reading is not obviously wrong; it
is just wrong.

Note this is also the *first* module whose rate is genuinely fixed. Bell 103
runs at the host rate; call progress does not.

## 42. `Dual_TONE_detect` is an answer-tone detector, not a call-progress one

Six verdicts, from a state 68 bytes long, decided by comparing three leaky
energy estimates:

```
    0   total energy below the floor -- nothing on the line
    1   energy present, neither tone dominates -- a person answered
    2   tone A present, held for less than 1280 samples (160 ms)
    3   tone A present and confirmed
    4   tone B present, held for less than 1280 samples
    5   tone B present and confirmed
```

The measurement is a Goertzel-style notch difference, the same trick
`FPM_TONE_detect` uses: run the signal through a notch, square both, and the
drop is the energy that was at the notch frequency. Three notches, all with
pole radius 0.9, at 0.2625, 0.225 and 0.28125 of the sample rate -- that is
**2100, 1800 and 2250 Hz** at 8000. Notch A alone gives tone A; notches B and
C are cascaded and give tone B together.

2100 Hz is the V.25 answer tone and the carrier for V.8 ANSam. 1800 and
2250 Hz bracket the FSK answer carriers -- V.21 channel 2 at 1650/1850 and
Bell 103 answer at 2025/2225 -- and the notches are wide enough (about
±127 Hz at r = 0.9) to cover them. So the two "tones" are *the far end
answered with a pure answer tone* and *the far end answered with an FSK
carrier*, which is exactly the distinction `CALLPROG_MODEM_ANSWER`,
`CALLPROG_V8BIS_MODEM_ANSWER` and `CALLPROG_VOICE_ANSWER` need.

Dial tone, ringback and busy are not detected here at all. That is cadence.c's
job, and it uses an entirely different filter bank.

Two details worth keeping:

- The energy floor `Dual_TONE_create` installs is **1**. Not 1000, not a
  fraction of full scale -- one. The "no signal" branch is very nearly
  unreachable, so a detector fed anything at all will report 1 rather than 0.
- The sample index is truncated to 16 bits every iteration (`cwtl` inside the
  loop), so a block longer than 32767 samples would loop forever. Nothing in
  the library passes one.

## 43. CPfiltrs.c and Elliptic1/2/3.c contain no code

The translation-unit bracket that holds the call-progress code is nineteen
files deep and mostly unanchored, so which symbol lives in which file has to
be argued rather than read off.

The FILE order is `toneiir.c`, `Cadence.c`, `CPfiltrs.c`, `Elliptic1.c`,
`Elliptic2.c`, `Elliptic3.c`. The `.text` blocks in that range are, in
address order, `toneiir_*` at 0x7c330, `_iir_filter_*` at 0x7c960 and
`cadence_*` at 0x7cd80 -- three blocks for six files.

`.rodata` settles it, because each file's *anonymous* statics can only be
referenced from its own translation unit, and `ld -r` concatenates `.rodata`
in the same order as `.text`:

```
    0x6240  toneiir_configuration_allpass                toneiir.c
    0x626e  CP_450_630 / CP_276_504 / CP_100_550 / CP_350_600   CPfiltrs.c
    0x6360  Filter_350_500 x 7                           Elliptic1.c
    0x6540  Filter_100_550 x 7                           Elliptic2.c
    0x6720  Filter_276_504 x 7                           Elliptic3.c
```

Monotonic, and in FILE order. Since `cadence_*` sits at a *higher* address
than `_iir_filter_*` while `Cadence.c` precedes `CPfiltrs.c`, `_iir_filter_*`
cannot be in `CPfiltrs.c`. It is in `toneiir.c`; `CPfiltrs.c` and the three
`Elliptic` files are pure data.

The semantically obvious answer -- "`CPfiltrs.c` is called CP*filters*, so the
filter engine is in it" -- is the wrong one. Worth remembering: in this object
the file names describe what a file *holds*, and a file can hold only tables.

### What the four CP_* designs are for

They are a per-country filter bank, and `cadence_create` is the single
function that reads them -- along with all nine `Filter_*` symbols in
Elliptic1/2/3.c. Twenty-one tables, one caller.

The measured passbands match the names exactly (`CP_450_630` is 396 to 670 Hz,
which is busy and congestion at 480 + 620), and finding 44 supplies the
selector: `GetDialToneCallProgressFilterIndex`,
`GetBusyToneCallProgressFilterIndex`, `GetRingbackToneCallProgressFilterIndex`
and `GetCongestionToneCallProgressFilterIndex` each choose which design
detects that tone, with `GetDialToneFilterSubindex` choosing among the seven
variants of a `Filter_*` family. A British modem and an American one listen
for busy tone through different filters.

(An earlier version of this section said the `CP_*` tables were unreferenced.
That was a broken query believed on a negative result -- see the retraction at
D10.)

---

## 44. The parameter numbering is recoverable from slmodemd

`modem_get_param(modem, n)` appears in the object as a bare immediate, and
until now every one has been an unknown. slmodemd still ships the enum they
index — `enum MODEM_PARAMETER_NAMES` in `modem_param.h` — and the names are
specific enough to read the call-progress code almost directly:
`GetMinBusyCadenceOnTime`, `GetDialToneCallProgressFilterIndex`,
`GetRingbackDetectionCyclesNumber`, `GetCallingToneFlag`.

The whole table is in `docs/parameters.md` and the constants in
`include/dsplib/modem_params.h`. Sixteen of them are per-country homologation
settings, carried in `struct homolog_params`.

**There is one trap.** The enum contains an alias, `MDMPRM_RATE =
MDMPRM_RX_RATE`, and the enumerator *after* an alias continues from the
alias's value plus one. Index it as if the alias consumed nothing and
everything from `MDMPRM_TX_RATE` onward shifts down by one — which is
self-consistent, plausible, and wrong. Four uses in the object pin it:

```
    dp_param_get           10  MDMPRM_DPRUNTIME       (tested in phase 1)
    call_create            7   MDMPRM_DIALSTR         (checks it starts with a digit)
    call_create            17  GetPulseDialMakeTime
    CALLPROG_Dial          27  GetCallingToneFlag     (gates the calling tone)
```

This also settles what `CALLPROG_Create`'s configuration block is for, and it
should make cadence.c substantially easier: its magic numbers are named.

## 45. CallingTone.c was written for 9600 Hz and is used at 8000

Three of its constants only make sense at 9600:

```
    phase step 2219 of 16384    1300.2 Hz at 9600   V.25 calling tone exactly
                                1083.7 Hz at 8000
    on  5760 samples            0.60 s at 9600      V.25 wants 0.5 to 0.7
                                0.72 s at 8000      outside it
    off 16800 samples           1.75 s at 9600      V.25 wants 1.5 to 2.0
                                2.10 s at 8000      outside it
```

Call progress runs at a fixed 8000 (finding 41), so all three land outside
V.25's tolerances, and the tone is 1084 Hz rather than 1300.

This is the first module found to carry a rate assumption that the code around
it does not satisfy, and it is worth noting *how* it was found: not by reading
the module, which is short and looks fine, but by asking what its magic
numbers would mean at each of the rates the library uses. 2219/16384 is a
meaningless constant until it is 1300.2 Hz.

Whether it was ever right is unknowable from the object alone. What is certain
is that as shipped the calling tone is the wrong frequency with the wrong
cadence, on top of being the wrong shape (D11) at the wrong level (D13) -- and
that one of slmodemd's fifty country configurations, CZECH_REPUBLIC, turns it
on.

---

## 46. toneiir's configuration, and an unresolved question about it

`toneiir_create(state, cfg)` copies 44 bytes of configuration into the head of
its 168-byte object and hangs the coefficients off pointers rather than
copying them, which is what distinguishes it from `_iir_filter_create` in the
same file. The layout, from `toneiir_create` and `toneiir_progress`:

| off | type | default @.rodata+0x61a0 | `toneiir_configuration_allpass` @0x6240 |
| --- | --- | --- | --- |
| +0x00 | `const short *a` | → 0x61ee | NULL |
| +0x04 | `const short *b` | → 0x61d6 | → 0x626c |
| +0x08 | `int n_a` | 12 | 0 |
| +0x0c | `int n_b` | 12 | 1 |
| +0x10 | `short` | 500 | 500 |
| +0x14 | `int` | 142539 | 142539 |
| +0x18 | `int` | 80 | 80 |
| +0x1c | `int` | 2200 | 2200 |
| +0x20 | `int` | 4 | 4 |
| +0x24 | `int` | 0 | 0 |
| +0x28 | `const short *scales` | → 0x61cc | NULL |

The object beyond the configuration is `short x[25]` at +0x30, `short y[25]`
at +0x62, then a group of counters and a threshold at +0x94 onward.
`toneiir_progress` filters exactly four biquads, hand-unrolled, ignoring `n_a`
and `n_b` entirely — the same shape as `_iir_filter_progress`, which is why
the two live in one file.

### Two things that did not add up, and how they resolved

**The default configuration's filter is all zeros.** The arrays at 0x61cc,
0x61d6 and 0x61ee are 5, 12 and 12 words of nothing. A filter with a zero
numerator outputs silence. So `toneiir_create(state, NULL)` — the "give me the
default" path — builds a filter that cannot pass a signal.

**`toneiir_configuration_allpass` has a NULL scales pointer**, and
`toneiir_progress` dereferences it before the first sample. It also declares
one numerator tap with the value 1, which in the Q13 the engine uses is a gain
of 1/8192, not the unity an all-pass implies.

Both readings are from relocations and are not in doubt — the pointers are
`R_386_32 .rodata` entries at 0x61a0, 0x61a4, 0x61c8 and 0x6244. What is in
doubt is what the caller does with them. `cadence_create` is the only user of
`toneiir_configuration_allpass` and is 2994 bytes; it is likely that it
overwrites the pointers after `toneiir_create` returns, using the config as a
template for the fields it does not want to set itself. That is the next thing
to establish, and it should be settled before any of toneiir is reconstructed
— building it against the wrong reading of these two tables would be a lot of
work to unwind.

### What the interval count proves

`toneiir_create` turns the configured duration into a count of intervals with

```
    need = (GetFP_Value(8, interval) * duration_ms) >> 14
```

`GetFP_Value(a, b)` is `ceil(a << 14 / b)`, so this is
`duration_ms * 8 / interval` -- and the 8 is **samples per millisecond**.
2200 ms at 500 samples an interval gives 35. The literal is a third
independent statement that this module runs at 8000 Hz, alongside
`call_create`'s resampler choices and `CALLPROG_Create`'s `imul $0x1f40`.

(Recorded because the relocation query that produced this nearly went the
other way: a malformed `readelf -r` filter reported *no* relocations in the
region, which would have made the configuration look like a struct of plain
integers with three implausibly similar values around 25000. Those "values"
are addresses. Anything that looks like a suspiciously narrow range of large
integers in this object is worth re-checking against the relocation table
before it is interpreted -- and see D10, where the same class of mistake was
made a second time.)

---

## 47. The cadence detector, and the level it needs

`cadence_progress` is the piece that tells dial tone from ringback from busy.
They share frequencies; what separates them is rhythm. It takes one sample,
hands it to a `toneiir`, and does nothing at all until that returns a verdict
— every 500 samples — so every duration it deals in is a count of 62.5 ms
intervals, not samples.

At each transition it records the period that just ended:

```
    silence -> tone   off[n] = run
    tone -> silence   on[n]  = run, then n++
```

`off[0]` is however long the detector happened to be listening before
anything happened, which is why every comparison below skips it.

### Three matchers, not one

Once `n` reaches the configured cycle count, the recorded periods are matched
— in one of three separately written forms, selected by two flags:

- **fixed** (`+0x2d0` set): `on[0]`, `off[1]`, `on[1]`, `off[2]` against four
  configured values, each within tolerance.

- **looped** (`+0x2ac` set): the last period must fall inside the timing
  windows, every earlier period back to `cycles` ago must agree with it, and
  then two final comparisons against the period `cycles` back — of which the
  silence test is **one-sided** (`off[last] > off[first+1]` fails) and the
  tone test is two-sided. Not a symmetry anyone would write on purpose, and
  reproduced as found.

- **unrolled** (neither): two patterns, either of which will do. A one-period
  cadence — the last three tones alike and the last three silences alike — or
  a **two-period** one, where the tone matches the period two and four cycles
  back rather than the one immediately before. That second form is what a
  double ring is, and a one-period test rejects it. It needs six cycles of
  history and is skipped below that.

The tolerance is `GetBusyToneDiffTime`, clamped to a minimum of three
intervals whatever the country table says.

### It needs the signal kept below about 8000

Measured, driving CP_450_630 with a 550 Hz tone and watching the interval
envelope:

```
    amplitude   envelope over 40 intervals   verdicts
      500          295 ..   304              8 present, 32 absent
     1000          596 ..   610             38 present,  2 absent
     5000         3278 ..  3335             38 present,  2 absent
     8000         5267 ..  5355             38 present,  2 absent
    12000         6995 .. 17438              4 present, 36 absent
```

At 12000 the cascade wraps internally — its passband gain is about 42 dB and
the interstage shifts only take that back out at the end — so the envelope
stops being steady, the stability test fails, and the detector reports the
tone as absent. **A loud busy tone is not detected.**

This is the same shape as the answer-tone detector's ceiling (finding 42) but
it bites four times lower, and unlike that one it is inside the range a real
line can produce. Whatever keeps the level down is upstream of call progress
and is not in this module; establishing what, and whether the margin is
adequate, belongs with `CALLPROG_Progress`.

---

## 48. `cadence_create`, and the units the country table speaks

`cadence_create(struct cadence *c, struct cadence_setup *s, int extra, void *modem)`.
`s` is a seven-word descriptor the caller builds on its stack; `s[4]` selects
which tone the detector is for, and the object names them itself, in a table
of debug strings at `.rodata+0x6208`:

```
    0  BUSY      1  DIAL      2  CONG      3  RING      4  INVALID
```

`cadence_create` clamps `s[4]` to 4 and **writes the clamped value back into
the caller's descriptor** before using it to index that table.

Each of the four cases reads its own parameters and they are the obvious ones
-- `GetMinBusyCadenceOnTime` and its three siblings for BUSY,
`GetRingbackDetectionCyclesNumber` for RING, and so on. DIAL is the odd one:
it has no cadence to match, so instead of timing windows it sets
`continuous`, and every interval in which the tone is present reports a
detection. It reads `GetDialToneValidationTime` in place of the cadence
windows.

### The filter bank, indexed

`Get*CallProgressFilterIndex` runs an eight-way jump table:

```
    0  Filter_350_500[sub]      4  Filter_100_550[sub]
    1  Filter_100_550[sub]      5  Filter_100_550[sub]
    2  Filter_350_500[sub]      6  CP_450_630
    3  Filter_276_504[sub]      7  CP_100_550
                              >7  CP_350_600
```

The three `Filter_*` families hold seven designs each, and
`GetDialToneFilterSubindex` picks one -- **one-based**, so `sub` runs 1 to 7
and the address arithmetic is `base + 24 * (sub - 1)` for the coefficients and
`base + 10 * (sub - 1)` for the scales. A subindex outside 1..7 falls back to
`CP_350_600`, the same default as an out-of-range filter index.

### The units, which were not obvious

Two conversions settle what the country table's numbers mean.

`GetCallProgressSamplesBufferLength` -- default 666 when the table says zero
-- becomes the `toneiir` **interval**, so a verdict arrives every 666 samples,
which at 8000 Hz is 83.25 ms.

Each of the four cadence windows is then converted by

```
    intervals = (GetFP_Value(1, buflen) * time * 80) >> 14
```

and `GetFP_Value(1, b)` is `ceil(16384 / b)`, so that is `time * 80 / buflen`.
For this to be a count of intervals, `time * 80 / buflen` must equal
`time_seconds * 8000 / buflen` -- which makes **the country table's cadence
times units of 10 milliseconds**. A 500 ms busy tone is 50 in the table and
six intervals in the detector.

The `toneiir` envelope floor comes from
`Get_Detection_Threshold_Table(GetDialToneDetectionThreshold)`, which is what
that sixteen-entry table is for, and is written into the configuration as a
16-bit store over the low half of an `int` field.

### Only two of the four are built

`CALLPROG_Create` calls `cadence_create` exactly twice, with `s[4]` of 0 and
1: a BUSY detector and a DIAL detector. Nothing in the object constructs a
RING or CONG detector, although both cases are fully written and both sets of
country parameters are read when they are. Whether ringback is detected some
other way, or simply is not detected, is a question for `CALLPROG_Progress`.

---

## 49. Each filter bank falls back to its own default, and a near-miss

`cadence_create`'s eight-way filter dispatch selects a `Filter_*` bank for
five of its indices, and each of those needs a subindex in 1..7 to pick one
of the bank's seven designs. **slmodemd returns 0 for
`GetDialToneFilterSubindex` unconditionally** -- the field is commented out of
`struct homolog_params` and the parameter is answered with a literal `return
0;` -- so in practice every bank selection falls back.

Measured against the blob, by calling `ref_cadence_create` and reading the
pointer it installed:

```
    index   subindex 0 gives        subindex 1..7 gives
      0     CP_350_600              Filter_350_500[sub-1]
      1     CP_350_600              Filter_100_550[sub-1]
      2     CP_350_600              Filter_350_500[sub-1]
      3     CP_276_504              Filter_276_504[sub-1]
      4     CP_350_600              Filter_100_550[sub-1]
      5     CP_350_600              Filter_100_550[sub-1]
      6     CP_450_630              (not a bank)
      7     CP_100_550              (not a bank)
     >7     CP_350_600              (not a bank)
```

Bank 3 falls back to `CP_276_504` -- its own nearest equivalent -- where the
other two fall back to the generic `CP_350_600`. That asymmetry is deliberate
and sensible, not an oversight.

### The near-miss

An earlier reading of the same code had bank 3 installing **NULL** coefficient
pointers, which `toneiir_progress` dereferences on its first sample. Ten of
slmodemd's fifty countries select bank 3 for dial tone -- Latvia, Turkey,
Jordan, Egypt, Lebanon, Malta, Morocco, Portugal, South Africa, UAE -- so that
would have been a null-pointer crash on a fifth of the shipped
configurations, and it was about to be written up as one.

It is not true. The three `mov $0x0,%reg` instructions that looked like NULL
each carry an `R_386_32 CP_276_504_*` relocation, and the greps used to trim
the disassembly had dropped the relocation lines, which `objdump -d -r` prints
separately and indented with tabs.

**This is the third time output filtering has caused a wrong reading here:**

```
  finding 46   the toneiir configuration's three pointers read as integers
               near 25000, because a readelf filter found no relocations
  D10          the CP_* tables read as unreferenced, because a query
               returned nothing for every input
  here         a fallback read as NULL, because a grep dropped relocations
```

Three of the same shape is a tooling problem, not three lapses of attention,
so it is now a tool: `tools/dis.py` disassembles a range with every relocation
folded into the instruction line it belongs to, leaving nothing that can be
accidentally filtered away. Use it instead of raw `objdump` for anything that
might touch a table.

What caught it was not care but measurement: the claim was tested against the
blob before being written down, and the test failed. That is the habit worth
keeping -- for a claim about what the original does, drive the original.

---

## 50. Cadence.c is complete, and busy tone is the one that refuses to fail

`cadence_create` is reconstructed. Three details were only settled by driving
the blob, and each of them was a plausible wrong guess first:

**The silence multiplier is not the subindex.** `cadence_create` keeps a
value on its stack -- 4 by default, 3 for busy -- which reads at a glance like
the filter subindex being defaulted per tone. It is not: it multiplies the
converted `max_off` to give `max_silence`, the gap after which the detector
throws away what it has measured. The subindex lives in a register, is zero
for every tone but dial, and is never defaulted at all.

Conflating them makes busy select a design out of a `Filter_*` bank where the
original falls back, which is a different filter with a different passband and
no test that only checks the cadence would notice.

**Congestion and ringback give up; busy does not.** All three substitute the
same default windows (20 to 550 ms) when the country table leaves any of the
four at zero -- but congestion and ringback then return NULL, freeing what
they have built, while busy carries on with the defaults.

That asymmetry looks like an oversight and reads better as a decision: busy
tone is the one a modem most needs to hear, since it is the difference
between redialling and waiting forever on a dead call. Congestion and
ringback are advisory. A country that does not specify busy timings gets a
detector anyway.

**Two fields are converted and never assigned.** `+0x274` and `+0x278` go
through the same 10 ms-to-intervals conversion as the four windows, and
nothing anywhere writes them, so on a freshly allocated object they are zero
in and zero out. Reproduced because they are part of the 732 bytes a
differential test compares.

### The Elliptic banks are reconstructed but unreachable

All twenty-one `Filter_*` designs are now in `src/callprog/elliptic.c` and
compared word for word against the blob. None of them is ever selected on a
shipped configuration: reaching one needs a subindex in 1..7, and slmodemd
answers `GetDialToneFilterSubindex` with a literal zero (finding 49). Three
banks of seven progressively wider bandpasses, fully designed, fully shipped,
and entirely dead unless a different host supplies that one parameter.

---

## 51. DialerConfig.c, and a calling convention that is not cdecl

`GetDialerConfig` fills a 60-byte struct with sixteen `modem_get_param` calls.
Fifteen are straight copies. The two that are not tell you what the numbers
mean.

### The DTMF levels are decibel tables

`GetDTMFHighToneLevel` indexes a seven-entry table directly, valid over 6..12,
and the entries are one decibel apart from 16384:

```
    6   16384    0 dB        10   10338   -4 dB
    7   14602   -1 dB        11    9213   -5 dB
    8   13014   -2 dB        12    8211   -6 dB
    9   11599   -3 dB
```

`GetDTMFHighAndLowToneLevelDifference` indexes a five-entry table of Q15
ratios over 1..5, giving -1 dB to -5 dB, and the low group is the high group
scaled by it. That is the DTMF twist.

Out of range, both fall back to **-2 dB** -- not to zero, and not clamped to
the nearest end of the table. Two decibels of twist is what the ITU asks for,
so the fallbacks are the specified values rather than arbitrary ones.

### `GetPulseBetweenDigitsInterval` is multiplied by ten

The only arithmetic in the function. It confirms the 10 ms convention
established for the cadence times (docs/parameters.md) outside cadence for the
first time: the parameter is in centiseconds and the dialler wants
milliseconds.

### AnalyseDialString is not called with the C calling convention

```
    7af5b:  movl $0x0,(%esp)        ; third argument, on the stack
    7af62:  mov  %ebx,%eax          ; first argument, in eax
    7af64:  mov  %esi,%edx          ; second argument, in edx
    7af66:  call 7a9f0 <AnalyseDialString>
```

and the callee reads them from exactly there. This is GCC's `regparm(2)`:
the first two integer arguments in `eax` and `edx`, the rest on the stack. It
is used for intra-translation-unit calls in Dialer.c, and the call carries no
relocation, which is how you can tell it is intra-TU.

**This matters for the reconstruction ahead.** A differential test cannot call
`ref_AnalyseDialString` with an ordinary prototype -- it will pass arguments on
the stack that the callee reads from registers, and get garbage that looks
like a plausible failure. The declaration needs
`__attribute__((regparm(2)))`, and every Dialer.c function reached only from
inside Dialer.c has to be checked for the same treatment before it is called
from a test.

This is the first non-standard convention found in the object. Bell 103 and
the call-progress modules are cdecl throughout, so nothing before now would
have shown it.

---

## 52. The dial-string parser, and three flags that mean the opposite of their names

`AnalyseDialString` grades a dial string on a four-point scale the object
names itself, at `.rodata+0x613c`:

```
    0 FATAL      too long to store
    1 INVALID
    2 TOLERABLE  oddities, but dial it anyway
    3 VALID
```

`IsDialStringInvalid` is exactly `grade <= INVALID`, so the name is precise.

The mode letter is mandatory and leading. A string starting with anything but
`T`, `t`, `P` or `p` is not examined at all -- it returns `VALID` if empty and
`INVALID` otherwise -- so `"5551234"` is rejected and `"T5551234"` accepted.
Whatever parses the AT line is expected to have put the letter there.

After it, an 88-entry jump table over `c - ' '` sorts characters into eight
classes: digits and `* # !`; the pause set `space $ ( ) , - @ W w`; `A`–`D`;
`T`; `P`; `^`; `;`, legal only as the last character; and 52 characters that
are simply illegal.

### The length limit is the buffer

100 characters, and the reason is visible in the object layout: the dial
string occupies bytes 0 to 99 of the dialler and the configuration begins at
+0x64, which is 100. `DialerCreate` copies the string in with `sysdep_strcpy`
and nothing bounds it, so the grade is the bound.

### Three flags test inverted

```
    cfg.abcd_permitted  != 0   ->  A-D are ILLEGAL
    cfg.mixed_permitted != 0   ->  switching T/P mid-string is ILLEGAL
```

Both read as the negation of their names. Rather than assume the code is
wrong, check what ships: 49 of slmodemd's 50 countries leave
`ABCDDialingPermittedFlag` at zero, and `modem_param.c` answers the mixed flag
with a literal `return 0;`. So both features are *enabled* almost everywhere,
which is the sensible behaviour -- the names are what mislead, not the code.

The third is genuinely counterintuitive and its polarity is not a naming
problem:

```
    cfg.modifier_validation == 0  ->  an unknown character makes it INVALID
    cfg.modifier_validation != 0  ->  an unknown character makes it TOLERABLE
```

Turning validation *on* makes the parser more forgiving. Twenty of the fifty
countries set it.

### It cannot be tested directly

`AnalyseDialString` is a file static -- `t`, not `T` -- so
`objcopy --redefine-syms` cannot give it a `ref_` name and there is nothing to
link against. That is also why it takes its arguments in registers
(finding 51): GCC picks its own convention for a function whose callers it can
all see.

It is therefore reached through `IsDialStringInvalid`, which collapses four
grades into a boolean and never exercises the `store` path. The
classification itself -- the part with the eight classes and the three flags --
is swept exhaustively through that caller: every byte value from 1 to 255, at
every combination of the flags. The grades the boolean hides are asserted
against the reconstruction alone and marked as such; `DialerProgress` is the
other caller and will close the gap.

This is the third file static that has had to be tested through a caller, and
the pattern is worth stating plainly: **a `t` symbol is not a testing
inconvenience, it is a signal that the calling convention may not be the C
one.** Check before writing the prototype, not after the comparison fails.

---

## 53. The pulse dialler

Five functions in call.c, and the only part of the library that operates a
relay rather than a filter. Dialer.c calls across to them; nothing else does.

`PulseDialDigit` loads a count, then `IsPulseDialerReady` is polled once per
5 ms tick and walks one cycle per pulse:

```
    elapsed <  break            hook on, line interrupted
    elapsed >= break            hook off, line restored
    elapsed >= break + make     one pulse done; count down, restart
```

returning true once the count reaches zero. The tick is a literal `5` in the
code, so every duration here is milliseconds -- unlike the cadence timings,
which are centiseconds (docs/parameters.md), and unlike everything in the
call-progress detector, which counts filter intervals. Three different time
units inside one phase, and none of them is stated anywhere.

Three details worth keeping:

- **A digit of zero is loaded as ten**, which is how loop disconnect has
  always spelled it.

- **`LastPulseDigitDialed` never touches the call object.** It only clears
  `MDMPRM_PULSE_DIAL`, the parameter `PulseDialDigit` set. So the *host* owns
  "a digit is being pulsed" and the library owns only the timing.

- **All five tolerate a missing datapump**, and `IsPulseDialerReady` answers
  *ready* rather than *busy* when there is none -- which is what stops a
  caller polling it from spinning forever on a torn-down modem. That is a
  deliberate-looking choice; the lazy version of this function would have
  returned 0.

### A configuration that dials nothing

With `break` set to zero, `elapsed < break` is never true, the line is never
interrupted, and the digit still counts down to completion on the make timer
alone. So a country table with a zero break time produces a pulse dialler that
appears to work and sends no pulses. Nothing in the code guards against it,
and `t_pulse` asserts it in that direction rather than treating it as a case
to avoid.

---

## 54. DialerCreate, and a bound that lives in another function

`DialerCreate` does not allocate. It takes an object, fills in the country's
rules, resets eleven fields, pushes the pulse timings through to the call
object, grades the string and copies it in:

```c
    d->grade = AnalyseDialString(d, s, 1);
    if (d->grade <= DIALER_INVALID)
        return 7;
    sysdep_strcpy(d->string, s);        /* into 100 bytes, unbounded */
```

The copy has no length check of its own. What keeps it inside the 100-byte
buffer is the check *inside `AnalyseDialString`*, which grades anything longer
`FATAL` -- and `FATAL` is 0, which is `<= INVALID`, so the function returns
before the copy. The two are a hundred lines and a function call apart, and
neither says the other exists.

It holds. But it holds by a coincidence of the grade ordering: if `FATAL` had
been given a value above `INVALID` -- which reads perfectly naturally, since
it is the *worse* condition -- the length check would still run, still grade
correctly, and the copy would overflow.

This is also the only call anywhere that passes `store = 1`, so it is the only
path that writes `d->last_digit`. That closed a gap the parser test could not
reach: `AnalyseDialString` is a file static (finding 52) and can only be
driven through its callers, and `IsDialStringInvalid` passes zero.

### DialerAbort acts in one state of sixteen

```c
    if (d->progress_state > 10)   return;
    if (d->pulse_released != 0)   return;
    if (d->pulse_active == 0)     return;
    LastPulseDigitDialed(d->modem);
    d->pulse_released = 1;
```

Three guards, and the only combination that does anything is a digit actually
being pulsed, not yet released, with the progress state in range. `t_dialer`
sweeps all fifty-two combinations of the three and asserts both that the right
one acts and that the other fifty-one leave the object untouched and tell the
host nothing.

---

## 55. The 221 callback is an S-register, and CALLPROG_Delete does not free

Two things settled by reading `CALLPROG_Create` and `CALLPROG_Delete`, ahead
of reconstructing them.

### S221

`CALLPROG_Dial` fetches the calling tone's level through a function pointer in
its object at +0x20, with index 221 -- which is outside
`enum MODEM_PARAMETER_NAMES` and had been an open question since finding 44.

The pointer comes from `CALLPROG_Create`'s configuration block, and
`call_create` fills it with `call_GetSRegister`:

```
    2b60:  movzwl 0x8(%esp),%eax
    2b65:  mov    %eax,0x8(%esp)
    2b69:  jmp    modem_get_sreg
```

So it is an **AT S-register**, and slmodemd's `sregs[]` is 256 entries, so 221
is in range. Not a defect -- just a second namespace that nothing named.

The lesson is the one D10 already taught in another form: an index that looks
out of range is evidence that the *namespace* is wrong, not that the index is.
Both times the temptation was to write up a bug.

### CALLPROG_Delete leaks nothing, and frees nothing either

```c
    DialerAbort(&cp->dialer);
    if (cp->busy)   cadence_delete(cp->busy);      /* +0x64 */
    if (cp->dial)   cadence_delete(cp->dial);      /* +0x6c */
    if (cp->f7c)    _iir_filter_delete(cp->band);  /* +0x78, gated on +0x7c */
    cp->f70 = 0;
    cp->busy = 0;
    if (cp->dtmf)   Dual_TONE_delete(cp->dtmf);    /* +0x84 */
```

The CALLPROG object itself is never freed -- it belongs to `call_create`,
which embeds it. Of the four sub-objects it does free, only two have their
pointers cleared afterwards: `busy` and `f70`. `dial`, `band` and `dtmf` are
left dangling, so a second `CALLPROG_Delete` on the same object would free
them again.

**Reachable?** `call_delete` calls it once. But `CALLPROG_Delete` is a global
symbol and the asymmetry is invisible from outside -- two of five cleared is
the kind of thing that reads as deliberate until you count.

### And the RING/CONG question is answered

`CALLPROG_Create` calls `cadence_create` exactly twice, with `setup[4]` of 0
and 1 -- BUSY and DIAL. There is no third or fourth call. So the ringback and
congestion cases in `cadence_create`, both fully written and both reading
their own country parameters, are dead in this build. Ringback is detected, if
at all, by something other than a cadence detector.

---

## 56. Three national pulse-dialling conventions

`DialerProgress`'s pulse state turns the keypad position `GetNextDigit...`
produced -- `row * 3 + col`, so `1` is 0 and `0` is 10 -- into a count of loop
interruptions, and it does it three different ways depending on
`GetPulseDialDigitPattern`.

```
    pattern 1        pattern 2            pattern 3
    n + 1            n == 10 ?  1         n + 1, then
    11 -> 10                : n + 2       > 9 kept, 11 -> 10,
                                          otherwise 10 - (n+1)
```

Worked through in digits:

```
    digit    1  2  3  4  5  6  7  8  9  0
    pattern 1    1  2  3  4  5  6  7  8  9  10
    pattern 2    2  3  4  5  6  7  8  9 10   1
    pattern 3    9  8  7  6  5  4  3  2  1  10
```

Pattern 1 is the convention most of the world uses: *N* pulses for *N*, ten
for zero. Pattern 2 sends one more than the digit and one pulse for zero.
Pattern 3 sends *10 − N*.

**The shipped country table names them.** Of slmodemd's fifty sets, forty-eight
hold pattern 1, and the two that do not are:

```
    SWEDEN         pattern 2
    NEW_ZEALAND    pattern 3
```

Which is exactly what the arithmetic says they should be -- pattern 2 is the
Swedish scheme and pattern 3 is New Zealand's. The identification was made
from the instructions alone and the data agrees, which is about as good a
confirmation as this kind of work offers.

Any other value of the parameter -- including zero, which is what a country
table that never thought about it would hold -- leaves the pulse count at the
-1 the state initialised it to, and `PulseDialDigit` is called with that.
Since `PulseDialDigit` only special-cases zero, -1 is loaded as the count and
`IsPulseDialerReady` counts it down past zero, which does not terminate.

**Reachable?** No. `GetPulseDialDigitPattern` comes from
`struct homolog_params::PulseDialDigitPattern`, and every one of the fifty
shipped sets holds 1, 2 or 3. A country table that left it at zero would hang
the pulse dialler; none does.

This is the second place where the library encodes something genuinely
national rather than technical -- the first being the cadence windows. It is a
reminder of what "homologation" meant: not a compliance checkbox but a real
per-country dialect that the modem had to speak.

---

## 57. DialerProgress is a generator, and what the comma limit is for

`DialerProgress` is not a state machine that gets polled -- it produces audio:

```c
    int DialerProgress(struct dialer *d, short *buf, int *pos, int limit);
```

It fills `buf` from `*pos` up to `limit`, advancing `*pos`, and returns a
status. The first thing it does is `if (*pos > limit) return 0`, and every
state either returns or updates `d->progress_state` and loops.

That explains a shape that made no sense while the signature was unknown: two
of the eleven states do nothing but write zeros. The dialler emits its own
silence -- pauses, inter-digit gaps, the waits after a comma -- rather than
asking anything else to.

### The states

```
    0        ask GetNextDigitAndReturnNextState what is next, and dispatch
    1        pulse-dial a digit: convert the keypad position with the
             country's pattern (finding 56), PulseDialDigit, then poll
             IsPulseDialerReady
    2, 4     emit `f_bc` samples of silence, clamped to what fits in the
             caller's buffer, carrying the remainder to the next call
    3        end of a pulse train: re-apply the make and break times and
             queue `cfg.pulse_gap * 8` samples of silence
    5 - 8    report an event (1, 2, 3 or 4) and go idle
    9, 10    release the line and report 5 or 6
    over 10  report 7
```

### `GetComaPauseDurationLimit`

The comma handler is where the last unexplained dialler parameter lands.
`GetNextDigitAndReturnNextState` counts a run of consecutive commas into
`d->repeat`, and this state turns that into

```
    total = cfg.pause * repeat        seconds
    if (total > cfg.coma_pause_limit)
            total = cfg.coma_pause_limit
```

So `,,,,,,,,,,` does not pause for ten times S8. The country table caps it,
which is what `GetComaPauseDurationLimit` is: a limit on how long a dial
string can make the modem sit silent holding the line.

That completes the reading of Dialer.c. Every parameter `GetDialerConfig`
fetches is now accounted for by something that uses it.

---

## 58. The DTMF generator, and how accurate it is

The tone half of `DialerProgress`'s dial state is a two-oscillator DTMF
generator, and its tables are the standard ones:

```
    rows     .rodata+0x5e72    697  770  852  941
    columns  .rodata+0x5e68   1209 1336 1477 1633
```

which is exactly the keypad `GetNextDigitAndReturnNextState` encodes into
`row` and `col` (finding 52's successor -- the matrix is in the task notes).
So the two halves of Dialer.c meet here: one turns a character into a grid
position, the other turns a grid position into two frequencies.

Each frequency becomes a phase increment for a 14-bit accumulator:

```
    increment = (hertz * 16777) >> 13
```

16777/8192 is 2.04797, and one cycle of a 14-bit accumulator at 8000 Hz is
16384/8000 = 2.048 increments per hertz. So the constant is that ratio in
Q13, and the generator reads the table as `TONE_read((phase + 4) >> 3)`.

### It is accurate, unlike the calling tone

```
    nominal   697.0   770.0   852.0   941.0  1209.0  1336.0  1477.0  1633.0
    actual    696.8   769.5   851.6   940.9  1209.0  1335.9  1476.6  1632.8
    error    -0.03%  -0.06%  -0.05%  -0.01%   0.00%  -0.01%  -0.03%  -0.01%
```

The worst is 0.06%, against a DTMF tolerance of ±1.5%. Which is worth saying
plainly next to D11: the *same* phase-accumulator idiom, in the same library,
written correctly here and wrong in `CallingTone.c` -- where the shift is 6
rather than 3 and the output is a sawtooth. Whoever wrote this one knew what
they were doing; the calling tone is not a limitation of the technique.

## 59. The dialler's generator: the parser chooses the state

`DialerProgress` is a switch on `d->progress_state` over eleven states, and
the surprise is where those numbers come from. There is no mapping step:
`GetNextDigitAndReturnNextState` returns the state number directly, and every
site that consults the parser writes the answer straight into
`progress_state`. So the parser's vocabulary and the generator's are one
vocabulary, and reading either half alone hides that.

| state | reached by | what it does |
|---|---|---|
| 0 | nothing in flight | ask the parser, set up whatever it names |
| 1 | a digit | DTMF burst, or one pulse train |
| 2 | *(nothing -- see below)* | inter-digit gap |
| 3 | `!` | hook flash |
| 4 | `,` | comma pause |
| 5 | `W` | return `DIALER_WAIT_DIALTONE` |
| 6 | `@` | return `DIALER_WAIT_ANSWER` |
| 7 | `$` | return `DIALER_WAIT_BONG` |
| 8 | `^` | return `DIALER_CALLING_TONE` |
| 9 | `;` | return `DIALER_COMMAND` |
| 10 | end of string | return `DIALER_DONE` |

Which settles the return codes, and they are not what their shape suggests.
Every code above zero is one modifier character, in the order the jump table
at `.rodata+0x5fdc` happens to put them -- there is no code for "hook flash
done" and none for "pause finished", because states 3 and 4 finish into state
2 and the caller never hears about either. A dial string of `5!5` and one of
`55` return exactly the same sequence of codes; only the audio differs.

State 2 is the one nothing selects. The generator has a case for it, and both
of the parser-consulting sites have a `case 2:` branch that sets up an
inter-digit gap -- but no path through the jump table returns 2. Every
character maps to 1, 3, 4, 5, 6, 8, 9 or 10, or to "skip and read another".
State 2 is entered only by falling into it from states 1 and 3, never chosen.
Reproduced anyway, since the original's switch has it.

### The hook flash borrows the pulse dialler

State 3 is worth its own paragraph because it is not what a hook flash usually
looks like. There is no separate timer: it rewrites the pulse dialler's two
timings -- make time to zero, break time to `GetHookFlashTime * 10` -- dials a
single "digit" of one pulse, and then puts `GetPulseDialMakeTime` and
`GetPulseDialBreakTime` back before leaving. A flash is a one-pulse digit with
no make. This is also the only place `hook_flash` is read, and the `* 10` is
the same centisecond-to-millisecond conversion as `GetPulseBetweenDigitsInterval`.

### After the last digit there is no gap

The post-digit handler compares `d->pos` against `d->last_digit` -- the index
`AnalyseDialString` recorded for the final dialable character. Anything before
it is followed by an inter-digit gap; the last one is not, and the parser is
consulted immediately instead. That is what lets a trailing `;` or `W` take
effect the instant the final digit stops sounding, rather than a gap later.

### A zero tone time never elapses

The DTMF burst tests `GetDTMFDialSpeed` for zero before it does any of its
sample accounting, and if it is zero it skips the accounting entirely: the
burst then fills every buffer it is handed and the digit never finishes. Not
reachable from any shipped country table, so this is reproduced rather than
fixed, and noted here rather than in `deviations.md`.

That completes the decode of Dialer.c.

## 60. CALLPROG_Progress: one transition per buffer, decided sample by sample

The supervisor's per-buffer step does three things in order, and separating
them is what makes it readable:

1. every sample is offered to whichever cadence detector the current state
   listens to, and the verdict, the state's timeout and the line-clear
   timeout each get a chance to *request* a transition;
2. the state does something with the buffer -- dial into it, put a calling
   tone in it, or judge whether the far end has gone quiet;
3. the requested transition, if any, is taken.

Nothing in steps 1 and 2 moves `cp->state`. Every sample in a call is judged
against the same state, and the caller sees at most one transition per
buffer. `request_state` refuses a second request, and refuses any request
that would land on `last_state` -- which is what stops a detector that fires
on every sample from re-entering the state it is already in.

### The verdict is not cleared between samples

It is set once before the loop, not once per sample. So the first detector
hit in a buffer re-applies its transition on every remaining sample of that
buffer. It is harmless -- the second request is refused, and the message and
`event` are simply rewritten with the same values -- but it is not what the
shape of the code suggests, and a reconstruction that resets the verdict each
sample diverges the moment a detector fires near the start of a buffer.

### `CALLPROG_DIALING` is set and then dropped

The dialler's `DIALER_BUSY` -- still dialling -- sets a local code of 3,
`CALLPROG_DIALING`. The tail then copies the code into the message only for
4, 12 and 14. Three is not in that list, so the message is discarded and
`CALLPROG_DIALING` is never reported from this path. Reproduced.

### The band filter is switched off by dial tone

`cp->dialtone_seen` is latched the first time the dial-tone cadence detector
asserts, and the band filter at the top of the function is applied only while
it is clear. The filter is there to help find dial tone; once found, it would
only colour what follows.

### Waiting for an answer is waiting for silence

State 8 is `CALLPROG_WFS_STATE`, and the object's own name gives the game
away: wait for silence. There is no tone to detect. The buffer is passed
through a rectifier and a single-pole smoother -- `env = |x/100| +
|0.99*env|`, both in Q14 -- and a buffer whose final envelope is at or below
99 counts as quiet. Five seconds of consecutive quiet buffers
(`40000 / count` of them) means the far end has picked up, and the machine
goes back to state 2 to carry on dialling.

### The states have their own names

The object carries them as debug strings, so they are recovered rather than
guessed: `CALLPROG_NO_LEGAL_STATE`, `CALLPROG_WAIT_DIAL`, `CALLPROG_DIALING`,
`CALLPROG_WAIT_RING`, `CALLPROG_WAIT_TO_ANSWER`, `CALLPROG_ANSWER_STATE`,
`CALLPROG_END`, `CALLPROG_END_PARTIALLY_STATE`, `CALLPROG_WFS_STATE`,
`CALLPROG_BONGTONE_STATE`. They are prefixed `CPSTATE_` in the
reconstruction, because the original's two enums overlap: `CALLPROG_DIALING`
is state 2 and, separately, message 3.

### The detectors were inert, and why

For a long stretch this function's differential test passed with **neither
cadence detector ever asserting**. The cause was not the tone, the cadence,
the filter bank or the level -- all of which were swept -- but a single
parameter.

`GetDialToneDetectionThreshold` is a level in dB, and `cadence_create`
converts it to a linear threshold exponentially. Measured through a detector
built by `CALLPROG_Create`:

```
    parameter    0     1     5    10    20    30    40    50    60   100
    threshold  16319 16324 16340 16356 16378  560   102    0   8192  7765
    asserts?     no    no    no    no    no   yes   yes   yes   no    no
```

Below about 30 the conversion wraps to roughly 16324 -- a threshold no signal
can reach. The test had been setting the parameter to 1, so every detector it
built was unreachable by construction, and every check passed because both
sides were equally deaf. Realistic country values are 30 to 50.

Two further things had to be right at the same time, and each was wrong on
its own first:

- **550 Hz, not 425.** The default call-progress filter is the 450--630 Hz
  band, so a 425 Hz tone -- the obvious choice for a European busy tone -- is
  outside it.
- **The cadence windows are in units of 10 ms and are converted**:
  `intervals = time * 80 / buflen`, so the country table's 4 and 12 become 2
  and 6 intervals of 20 ms. `t_cadence` is not a guide here, because it
  hand-builds its cadence object and never calls `cadence_create` at all --
  its 4 and 12 are already intervals.

With the threshold at 40 the machine comes alive: `CALLPROG_DIALING` appears,
which can only be reached by the dial-tone detector asserting in state 1, and
the run visits state 2 more than four times as often. Everything still
matches the blob sample-for-sample, which is the point -- it means the
verdict-to-event path, including the fact that the verdict is not cleared
between samples, is now verified rather than merely written.

`t_callprog_progress` guards this directly: it requires
`CALLPROG_DIALING` to have been reported at least once, so a future change
that quietly deafens the detectors fails instead of passing.

## 61. call.c: the datapump that runs before there is a connection

`DP_CALL` is registered from `dp_call_init` and runs between going off hook
and having a carrier. All the judgement is `CALLPROG_Progress`; this module
dials, feeds the supervisor 48 samples at a time whatever size the host asks
in, and turns the message into a `DPSTAT_*` code.

### Reaching it at all

All four functions are file statics, so `objcopy --redefine-syms` cannot give
them `ref_` aliases and there is no way to call them by name. The way in is
the operations table: `dp_call_init` is global and passes `&call_op` to
`modem_dp_register`, so calling it and taking the pointers out of the
harness's registration log reaches every one of them. Finding it needed the
in-place addends -- these are REL relocations against `.text`, so `objdump -r`
shows no addend at all and the four references only appear once the four
bytes at each relocation site are read out of the section.

### The message-to-status map

```
    CALLPROG_NO_RING, NO_ANSWER, ANSWER_STATE_TIMEOUT  -> DPSTAT_NOANSWER
    CALLPROG_NO_DIAL_TONE                              -> DPSTAT_NODIALTONE
    CALLPROG_BUSY, CONGESTION                          -> DPSTAT_BUSY
    CALLPROG_ERROR                                     -> DPSTAT_ERROR
    CALLPROG_ANSWER, MODEM_ANSWER, VOICE_ANSWER,
      V8BIS_MODEM_ANSWER, and messages 16 and 17       -> DPSTAT_CHANGEDP
    everything else                                    -> DPSTAT_OK
```

Which settles what messages 16 and 17 are, the two the object does not name:
they are handled exactly like the four answer messages, latching `answered`
and asking the host to change datapump. They come from the automode dual-tone
detector, so they are answer-tone verdicts.

`DPSTAT_CHANGEDP` also latches `answered`, and from then on the incoming
block is discarded rather than listened to and the outgoing side is filled
with silence -- the line belongs to whatever datapump takes over.

### Two rings and a double buffer

The host hands over arbitrary sample counts and the supervisor wants fixed
48-sample blocks, so each direction has a 192-byte ring the host's samples
flow through plus a 96-byte double buffer the supervisor works in. The
`active` field of each queue holds a byte offset that is only ever 0 or 96,
and both flip after every block. The original tests them with `cmp $1` and
`sbb`, which reads as a comparison but is only ever distinguishing those two
values.

The outgoing side is primed at create with one block of silence and its
`active` pointed at the *other* half, so the first call has something to send
before it has processed anything. That is a one-block delay through the
datapump, by construction.

### The dial string gets a prefix if it has none

`MDMPRM_DIALSTR` is the host's string. If it starts with a digit it carries
no mode prefix, so one is put in front of it into a 64-byte stack buffer --
`p` or `t` according to S16. Anything else, and anything longer than 62
characters, is passed through untouched.

### The initial message is one past the end

`call_create` sets the remembered message to 18, which is
`CALLPROG_MAX_MESSAGES` -- not a message at all, but a value the first block
is guaranteed to differ from. Worth noting because 17 is a real message and
the off-by-one is invisible until a test runs fewer than 48 samples through
and never completes a block.

### It was waiting for a dial tone it was configured never to listen for

`t_call` first ran with the outgoing buffer silent in every configuration:
the supervisor never left `CALLPROG_WAIT_DIAL`, and only the dialling state
writes samples. Three plausible explanations were wrong -- the tone level
(the table really was half scale, and fixing it changed nothing), the
call-progress filter index (all eight behave the same), and the 48-sample
block size. Driving the built dial-tone detector directly settled it: 354
detections in 64000 samples. The detector worked; nothing was asking it.

`toneiir_dialtone_table` was **all zeros**, and `toneiir_busy_table` was zero
in exactly states 1 and 2 -- the inverse of what the machine looks like from
`CALLPROG_Create` alone. Both tables depend on `cfg.w0`, which `call_create`
derives from **S56**:

```
    S56 = 2, 4, ...  -> w0 = 0    dialtone  0 1 1 0 0 0 0 0 0 0
                                  busy      1 1 1 1 1 1 1 1 1 1
                                  CALLPROG_Dial starts in state 2

    S56 = 0, 1 or 3  -> w0 = 1    dialtone  0 0 0 0 0 0 0 0 0 0
                                  busy      1 0 0 1 1 1 1 1 1 1
                                  CALLPROG_Dial starts in state 1
```

So with `w0 = 1` no state listens for dial tone anywhere, and states 1 and 2
listen to nothing at all. That is blind dialling: the modem waits a fixed
time in `CALLPROG_WAIT_DIAL`, hears nothing because it is not listening, and
moves on when the timeout expires -- which is seconds, longer than the first
version of the test ran. `t_call` had S56 at 0.

This corrects `docs/callprog_states.md`, which recorded the `w0 = 0` shape as
though it were the only one. `t_call` now covers both halves of the split,
and with S56 = 2 the dialler's DTMF reaches the line -- 13840 non-silent
samples where there were none.


## 62. V.8, and why it comes before V.23

V.8 is the negotiation that runs once a call is answered and before any
datapump starts. It is pulled ahead of V.23 because it is the direct
successor to `call.c` -- which hands over with `DPSTAT_CHANGEDP` the moment it
hears an answer -- because its modulation dependency (V.21 FSK) is already
done, because SpanDSP ships an independent V.8 to cross-check against, and
because it is the smallest thing that makes an end-to-end negotiation
watchable over SIP. Nothing depends on V.23.

### The public surface is all global

Unusually for this object file, every V.8 entry point is a global symbol:
`V8Create`, `V8Delete`, `V8Process`, `V8Control`, `V8SetMessage`,
`V8GetMessage`, `V8UpdateModemParameters`, `V8agc`, `V8_setFilters`,
`V8_V21_reset`, and about thirty-four lower-case `v8_*` helpers. All of it
can be driven by name in a differential test -- the opposite of the
call-progress code, where the work lived in file statics reachable only
through a caller.

### The dependency order

`V8Create` calls exactly two things: `sysdep_malloc` and `v8handshakinit`.
That second one is the whole signal layer:

```
    V8Create
      +- v8handshakinit
           +- initTxSequence   v8_detectorinit   v8_phase_rev_init
           +- v8_rxinit        v8_txinit         v8_V21_Init
           +- v8_mpyint
```

So the build is bottom-up: arithmetic leaves, then the init functions, then
`v8handshakinit`, then `V8Create`, then the `v8.c` datapump wrapper -- which
is the same shape as `call.c`, registering under id 8 from `dp_v8_init` and
refusing any sample rate but 9600.

### The cosine table

256 entries of Q14 cosine, and the generator is
`(short)(16384.0 * cos(2 * PI * i / 256))` with C truncation towards zero.
255 of the 256 match that exactly.

The exception is index 128 -- half a cycle -- which holds -16383 where the
arithmetic gives -16384. That is floating point showing through: their cosine
returned slightly more than -1, so truncating dropped a unit. Reproduced
verbatim; regenerating the table would quietly change one sample of every
tone V.8 emits.

### Two corners in the leaves

`v8_absfn(-32768)` returns -32768, because negating it overflows and the
result is narrowed back to a short. `v8_copycoeff` counts with a short, so a
count above 32767 never terminates. Neither is reachable from the signal
path; both are reproduced.

`v8_crc` is CRC-16-CCITT, polynomial 0x1021, MSB first and unreflected, one
bit per call. Its bit argument is compared sixteen bits at a time, so a value
that is non-zero overall but zero in its low half counts as a zero bit.

## 63. The V.8 object, and how its layout is being pinned down

`V8Create` allocates 0xec4 -- 3780 bytes -- and hands it to `v8handshakinit`,
which parcels it out among the initialisers:

```
    v8_rxinit(v)                +0x01c..+0x0f6, +0x894..+0x9d2
    v8_txinit(v)                +0x004..+0x018, +0x110..+0x5be, +0x77c..+0x892
    v8_phase_rev_init(v+0xb40)  its own sub-object
    v8_detectorinit(...)        around +0xc54 and +0xcd4
    v8_V21_Init(v, 1, 0)        +0xdd8..+0xe5c
    v8_TONEq_init(v)            +0xdd2, +0xdd4
```

The regions are disjoint, which is what makes a bottom-up reconstruction
possible: each initialiser can be written and proved on its own before
anything that reads what it writes exists.

### The offsets are asserted, not trusted

Every struct in v8.h carries `offsetof` assertions in v8util.c, so a careless
edit stops the build rather than failing a test somewhere far away. They
earned their place immediately: the first version put the tone-queue shorts
at +0xdd2 and the V.21 pointers at +0xdd8 in one struct, and the compiler
quietly realigned the whole thing to a four-byte boundary. The assertion
failed on the spot. Splitting the tone queue out of `struct v8_v21` fixes it,
and is the better description anyway -- a tone queue and a V.21 modem are not
one object.

The assertions are compiled only where a pointer is four bytes. The object
holds pointers at fixed offsets, so its layout is a property of the
original's ABI and cannot hold on a 64-bit host; `make check64` compiles this
file only to keep the C portable.

### Testing an initialiser

The comparison that matters is the whole object, not the fields: fill 3780
bytes of both sides with the same non-zero pattern, run each initialiser, and
require every byte to agree. That catches a mis-stated offset, a missed
field, and a field written one byte too wide -- none of which a field-by-field
check would find unless it happened to name the field that moved. The
non-zero fill also means an initialiser that does nothing cannot pass by
agreeing with itself.

## 64. The V.8 transmitter and receiver, and what their pointers say

`v8_txinit` and `v8_rxinit` both take the whole object and between them claim
most of the low half of it. They are pure field writes, but the pointers they
plant are the interesting part, because a pointer into your own object is a
statement about how the buffer is used.

```
    tx_sym_a, tx_sym_b  -> tx_symbols[0]     both to the start
    tx_ring_base        -> tx_ring[0]
    tx_ring_half        -> tx_ring[64]       a quarter of the way in
    rx.buf              -> rx_scratch2
```

Two cursors half a buffer apart on the same ring is a shaping filter being
kept fed while the modulator drains behind it; two cursors to the *same*
place is a queue that starts empty. That is worth more than the offsets.

### The receiver clears a buffer and then writes into it again

`v8_rxinit` zeroes all 160 words of the receive scratch and then sets element
40 to 0x1000. Reproduced in that order. Doing it the tidy way round -- seed
first, clear after -- would zero the seed, and doing the clear as a `memset`
that stops short of element 40 would only work by luck of the index.

### Testing an initialiser that plants pointers

The whole-object byte comparison from finding 63 has a hole in it here: two
objects at different addresses cannot hold the same bytes where a pointer
into themselves lives. Each pointer is therefore compared as an offset from
its own base and then overwritten with that offset, after which the byte
comparison covers everything.

Only the pointers the call just wrote may be touched. The first version
normalised all five after every call, which meant reading the ones still
holding the fill pattern and doing pointer arithmetic on garbage -- and that
is how it came to dump core rather than merely report a mismatch. A test that
crashes is at least loud; the same mistake in the reconstruction would have
been a silent wrong answer.

## 65. v8_V21_Init, and a layout guess that a later function corrected

V.8's CM and JM ride on a 300 baud V.21 link, and `v8_V21_Init` configures
it. Two choices are made, and they are independent:

| choice | picks |
|---|---|
| `channel` -- which V.21 channel this modem sends on | the two carrier constants and which 61-tap filter is copied in |
| `answerer` -- whether this modem answered the call | the four filter designs and two more constants |

All four combinations are reachable and all four are tested. That two
separate things are selected here, rather than one "am I the caller" switch,
is the interesting part: the channel and the role are orthogonal in V.8, and
the code says so.

### It corrected an earlier guess about the object

`struct v8_phase_rev` had been given a size of 0x114 bytes, which was a guess
-- `v8_phase_rev_init` only writes up to +0xdc, and 0x114 was the distance to
the next thing then known. `v8_V21_Init` writes a block at +0xc20, which is
0xe0 into the phase-reversal detector. So the detector is 0xe0 bytes, not
0x114, and the V.21 parameters begin exactly where it ends.

This is the normal way the object gets mapped: a region's extent is a guess
until something else claims the space after it. The `offsetof` assertions are
what make the correction safe -- moving a boundary either still satisfies
every recorded offset or stops the build.

### Comparing two copies of the same coefficients

`v8_V21_Init` plants pointers to filter tables. One side's tables live in the
object file and the other's in `v8v21.c`, so the pointers can never match.
The test compares 48 coefficients through each pointer and then blanks the
four words, so the whole-object byte sweep still covers everything around
them. That is the same shape as the pointer-into-self problem in finding 64,
but the fix is different: those had to be compared as offsets, these have to
be compared by dereferencing.

## 66. initTxSequence: where V.8's negotiation becomes bits

This is the function the monitoring goal depends on. It reads the call menu
-- three flag bytes and two optional four-character extension fields -- and
writes the message out as V.21 characters, then records how long it came to.

### The entries are ten bits, already framed

The constants written into the buffer are 0x3ff, 0x107, 0x141, 0x161 and so
on: nine and ten bit values, not octets. What settles it is the length the
function stores at the end, which is the character count times **ten** -- a
start bit, eight data bits and a stop bit, with the framing already folded
into the constant rather than added by the modulator later.

Extension characters go through `charFlip` and then `(x << 1) | 1`, which
puts the same framing round a reversed octet. So the fixed characters and the
variable ones are the same shape, which is the confirmation that reading them
as ten-bit words is right.

### One function, both messages

`initTxSequence` takes only the object. Which message it builds, and where it
puts it, come from two pointers the caller sets first -- `v->cm` and
`v->tx_seq`. That is why `v8handshakinit` calls it twice with different
pointers and no other difference: CM and JM are the same encoder over
different menus.

### An extension declared but left empty un-declares itself

Both extension fields are copied up to four characters, stopping at the first
zero. If a field is marked present but its first character is zero, nothing
is emitted **and the present bit is cleared in the menu itself**. The call
function is then chosen by re-testing that same bit, so clearing it changes
which branch runs a few lines later. An initialiser that mutated its input
would be a surprise in most code; here it is load-bearing.

### The tail is chosen by two bits read as one word

The original tests `*(int *)cm & 0x80008` -- a single 32-bit read across the
flag bytes, which is bit 3 of the first byte or bit 3 of the third. Written
out as the two byte tests it plainly is, since a 32-bit read of a
three-byte-plus structure is only correct by accident of layout.

## 67. v8handshakinit: the map before the code

Every dependency of `v8handshakinit` is now reconstructed, so it is next.
This section records what it touches, because the function is not hard so
much as wide: 65 distinct fields written, most of them nowhere near anything
already mapped.

```
    written  0x9d4 0x9d6 0x9d8  0xa3e 0xa40
             0xc48 0xc4c 0xc50  0xc94 0xc96 0xc98
             0xcb2 0xcb4 0xcb6 0xcb8 0xcba 0xcbc 0xcbe 0xcc0
             0xcc4 0xcc8 0xccc 0xcd0
             0xd14 0xd16 0xd18 0xd1a 0xd1c 0xd1e
             0xd32 0xd34 0xd36 0xd38 0xd3a 0xd3c 0xd3e 0xd40
             0xd44 0xd48 0xd4c 0xd50
             0xd94 0xd96 0xd98 0xd9c 0xda0 0xda4
             0xdb4 0xdb6 0xdb8 0xdbe 0xdc0 0xdc4 0xdc8 0xdcc 0xdd0
             0xdd2 0xdd4  0xe5c 0xe60 0xe64  0xebc 0xebe 0xec0 0xec2

    read     0xa42 0xa44 0xa4c 0xa50 0xa58 0xd14 0xd16
```

The four reads at 0xa44, 0xa4c, 0xa50 and 0xa58 are the configuration
`V8Create` planted: 0xa44 selects between three whole shapes of handshake
(and returns early on one of them), 0xa4c and 0xa50 are timeouts converted by
`* 9600 >> 2`, and 0xa58 is the call menu. So the object arrives configured
and this function lays out the machine to match, which is why it is wide
rather than deep.

0xc48 and 0xc4c are written twice: once before the first `initTxSequence`
call and again before the second, with a third pointer at 0xc50 set in
between. That is the CM and the JM being built into different buffers by the
same encoder, which finding 66 predicted from the encoder's side alone.

### An argument type a differential test could not catch

Reading this caller corrected `v8_detectorinit`, whose third argument had
been declared `int`. It is passed a `.rodata` address. On a 32-bit target
both are four bytes and the value copies through either way, so every
differential check passed while the declaration said the wrong thing. Nothing
in a same-width comparison can find that -- only reading the caller can.

## 68. v8handshakinit, and three ways to read a table wrong

The map in finding 67 turned out to be the easy part. The function itself is
three shapes chosen by `v->mode`: 0 is the full handshake, 1 is the answering
side, and anything else returns having done only the preamble every shape
shares. That preamble includes `v8_rxinit` and `v8_txinit`, so the transmit
and receive pointers are set whatever follows -- which matters more for the
test than for the code.

### Five sequence buffers, confirmed rather than assumed

`struct v8_tx_sequence` was inferred in finding 66 from the encoder alone.
This function confirms it from the other side: it hand-builds a sequence at
+0xd14 without calling the encoder, and puts the terminator at +0xd32 and the
length at +0xd36 -- exactly where the struct says they go -- with a length of
60 for the six words written above them. A second hand-built one at +0xc94
has three words and a length of 30.

So the object holds five of them in a row, +0xc54 through +0xd94, and the
arithmetic closes: five times 0x40 lands exactly on the next named field.

### The flag word is assigned, not or-ed

Both shapes store a constant into `rx.flags` rather than or-ing one in, which
drops whatever `v8_rxinit` left there. Order therefore matters both ways:
mode 0 stores 0x8000 and *then* runs the detector initialiser, which ors in
its own bit; mode 1 runs `v8_V21_Init`, which ors in a bit, and *then* stores
0x8004 over the top of it. The same two lines in the other order would give
different answers in both shapes.

### The detector's table is eight entries, not a hundred

The table passed to `v8_detectorinit` sits at .rodata+0x5670, and the next
thing known was the cosine table at +0x5740, so a first pass copied the whole
0xd0-byte gap. It failed from index 8 onwards, and the values gave the reason
away: one side held small numbers and the other pairs that read as addresses.
Relocations begin at .rodata+0x5680. Everything past index 8 is a pointer
array belonging to something else, and a verbatim copy holds link-time
addends where the running object holds real addresses.

Three separate mistakes about this table in one sitting -- wrong type
(finding 67), wrong length, and a test that dereferenced it in modes that
never set it. The general lesson is the same each time: a region's extent is
never implied by the distance to the next thing you happen to know about.

### A test bug that had been latent for five commits

`whole_object` passed a label containing `%s` while handing the harness a
byte offset to format. It had been there since finding 63 and had never
fired, because the label is only formatted when a check fails and that
comparison had never failed. The first genuine mismatch turned it into a
segfault. Worth recording as the failure mode of a diagnostic path that is
only exercised by failure.

## 69. Two constructors were wrong, and the harness could not see it

`V8Create` allocates its object and never zeroes it. That prompted an
experiment: make the harness allocator hand back a fixed non-zero pattern
instead of whatever `malloc` had lying around. Fresh pages from the operating
system are usually zero, so a constructor that misses a field looks correct
for as long as nothing else has used that memory.

Two real defects surfaced within seconds of turning it on, in code that had
been green for weeks.

**FPM_TONE_create cleared one word too few.** The original clears its running
state in two loops ending at +0xf0, and then stores zero to +0xf2 in a
separate instruction just after. An earlier reading saw the loop bound, did
not see the separate store, and wrote a comment claiming the last word was
deliberately left alone. It is not. Corrected, and the comment retracted in
place.

**B103FP_create cleared twenty-four bytes too many.** The reserved block at
+0x2c is three twelve-byte entries, and the original clears only the first
ten bytes of each -- the last word of every entry keeps whatever the
allocator left. Clearing all thirty-six reads as tidier and is wrong.

Both are the same shape of error in opposite directions, and neither could
ever have been caught by a differential test against zeroed memory: with both
sides reading zeros, a missing write and a spurious write are equally
invisible.

### Why the fill is staged rather than enabled

Turning it on also made one half-duplex test exit before reporting -- eight
groups stopped appearing rather than failing, which is worse than a failure
because the suite still says zero failed. Something in that path reads memory
that has always happened to be zero. That is a third defect and wants its own
investigation, so the constant is defined in harness.h with the reasoning and
the fill is not applied yet.

A test that silently stops running is the failure mode to watch for here:
the group count is the only thing that shows it, which is why it is reported
alongside the check count every time.

## 70. V8Create, and the limit it puts on its own test

The constructor is short: allocate 3780 bytes, copy six configuration words
in, plant 0x4000 at +0xa42, call `v8handshakinit`, clear two fields. Most of
its 1124 bytes are debug output.

It does **not** zero the object. So only the six words, the constant, the two
final clears, and whatever `v8handshakinit` writes are defined when it
returns -- everything else is allocator leftovers. A caller reading anything
else is reading rubbish that, on a fresh page, is usually zero and therefore
looks deliberate.

That bounds the test. Comparing all 3780 bytes would compare two lots of
leftovers, so `t_v8create` compares what the constructor is responsible for
and leaves the rest to `t_handshakinit`, which proves the handshake
byte-for-byte on an object the test controls. Splitting it that way is not a
weakening: every byte is still covered by one test or the other, and neither
covers bytes nobody writes -- because nothing can.

Removing that limit is what task #23 is about.

## 71. v8handshak's shape, before reconstructing it

The last large function in phase 5 is 4243 bytes, which suggested a big state
machine. It is not: it is a small one with large state bodies.

The entry is a loop:

```
    while (v->f21c < v->fa3e) {          /* until the transmit queue is full */
        state = v->f9d4 - 5;
        if ((unsigned)state > 0x28)
            continue;                    /* unknown: keep waiting */
        goto state_table[state];         /* .rodata+0x5680, 41 entries */
    }
```

The table has 41 entries but only **six** distinct targets, and one of those is
the loop head itself -- 36 of the 41 entries point back at it. So five states
have bodies, and `v->f9d4` only ever holds:

| f9d4 | entry |
|---|---|
| 5 | 0x775e8 |
| 6 | 0x774d9 |
| 23 (0x17) | 0x7746a |
| 43 (0x2b) | 0x773d0 |
| 45 (0x2d) | 0x77363 |

Which agrees with everything already reconstructed: `v8handshakinit` sets
`f9d4` to 5 in its full-handshake shape and 6 in its answering one,
`V8Control`'s third request sets it to 0x17, and `V8Process` tests for 0x2b
when deciding what to report. The remaining 36 table entries are the
compiler filling the gaps in a switch whose cases are sparse, and they are
not dead code so much as "no such state".

The loop condition is the interesting part: the machine runs until it has
queued enough to transmit, so one call does as much work as the transmit
queue has room for rather than a fixed amount. That is why `V8Process` calls
it from inside its own per-sample loop and only when the queue is nearly
empty.

### What the five states transmit

| f9d4 | what it puts on the line |
|---|---|
| 5 | silence -- four zeros into the staging buffer, then queue |
| 6 | ANSam, `v8_ansamgenerate` inlined, until `deadline_a` |
| 23 | FSK, until `deadline_b` |
| 43 | FSK, with no deadline |
| 45 | the queued tone, `v8_TONEq_generate` inlined, then queue |

States 6 and 45 are the two generator functions inlined rather than called,
the same way `v8handshakinit` inlines `v8_ansaminit`. Nothing is lost by
having them as functions here: the object simply chose to inline them.

The two FSK states share a structure. Each modulates the current bit
(`v->fa3c`), advances a sample counter by four, and when that counter reaches
the bit length in `v21_params.f06` fetches the next bit with `v8_getbit` and
resets the counter. State 43 additionally counts bits and does something
after sixty of them; state 23 counts elapsed blocks against `deadline_b`
instead.

So the transmit side is: pick a signal, emit four samples of it, and pull a
new bit whenever the current one has been on the wire long enough.

### A second state variable drives the receive side

When the loop exits -- the queue is full -- the function dispatches again, on
`v->f9d6` rather than `f9d4`, and only when `v->f110` says at least six
symbols have arrived:

| f9d6 | |
|---|---|
| 0x19 | run `V8agc` and go on to the detectors |
| 0x20 | a second receive path |
| 0x23 | just `v8_rxreadqueue` |
| 0x28 | a third |
| 0x63 | return 2, which is what `V8Process` reads as "something changed" |

Two independent state variables, one per direction, is why a single 4 KB
function covers what looks like it should be two.

## 72. What the blob cannot be the judge of

Twenty-seven million differential checks establish one thing: the
reconstruction does what the object does. They cannot establish that the
object was read correctly in the first place. A misread frequency, an
inverted mark and space, a sign error in a filter -- each would make both
sides wrong in exactly the same way, and every check would still pass.

Two kinds of evidence answer the other question, and both are now in place.

### The constants are the standard's

Derived from the reconstruction's own integers, at the rate the handshake
runs at:

```
    ANSam carrier      0x0e00 / 16384 * 9600 = 2100.00 Hz
    ANSam modulation   0x001a / 16384 * 9600 =   15.23 Hz
    ANSam reversals    0x438 blocks * 4 / 9600 = 450 ms

    V.21 ch1 space     0x03ef /  8192 * 9600 = 1180.1 Hz
    V.21 ch1 mark      0x0344 /  8192 * 9600 =  979.7 Hz
    V.21 ch2 space     0x062b /  8192 * 9600 = 1850.4 Hz
    V.21 ch2 mark      0x0580 /  8192 * 9600 = 1650.0 Hz
```

V.8 specifies 2100 Hz amplitude modulated at 15 Hz with a phase reversal
every 450 ms; V.21 specifies 980/1180 and 1650/1850. These are not close to
those numbers, they are those numbers -- which also settles that the
handshake's sample rate really is 9600, since no other rate makes the same
integers come out right.

### An independent implementation agrees

`make interop` builds a separate 64-bit binary against SpanDSP, whose V.8 has
nothing to do with this object file, and runs the signal both ways:

- SpanDSP's own tone detector identifies our generated ANSam as
  `MODEM_CONNECT_TONES_ANSAM_PR`.
- SpanDSP's generated ANSam, resampled from 8000 to 9600 by the
  reconstructed converter, gives our phase-reversal detector eight
  reversals. That direction matters more than the first: it is what would
  catch a detector tuned to our own generator's quirks rather than to the
  standard.

The full V.8 state machine's verdict is printed but not asserted on in that
file, because nothing there replies to SpanDSP -- its negotiation cannot
finish however correct our ANSam is. A whole two-way negotiation is section
75 below.

### Two mistakes this found

The generator emitted silence at peak zero, because it runs through a
shaping filter whose taps only `v8_V21_Init` populates and the test had not
called it. Nothing in the differential harness would have noticed -- both
sides would have been equally silent, and equal.
## 73. Four kilobytes, and the half that a passing test did not reach

`v8handshak` reconstructed as 188 lines of C against 4243 bytes of x86 was
already suspicious, but the differential test said 114,572 checks and zero
failures, so nothing pushed back. What eventually did was `V8Process`: three
bytes of a 3780-byte object comparison disagreed, at `+0xc38` and `+0xdb8`.

Both are counters. `+0xc38` is the demodulator's bit count, and the
reconstruction had let it run to 29 where the original stopped at 9; `+0xdb8`
the original cleared and the reconstruction never touched. Searching the
whole object for stores to those two offsets found them in a region of
`v8handshak` between `0x77916` and `0x783a3` that the reconstruction did not
have at all -- roughly 2700 bytes, the entire message-matching half of the
receive path.

The reason the earlier test passed is that it could not reach any of it. The
matching half runs only with receive state `0x28`, and dispatches below that
on `f9d8`, which the sweep seeded at `0x19` or `0x24` and drove for thirty
blocks -- not nearly enough to leave the AGC state, whose own counter needs
0x960 blocks. Every check it made was real; none of them was of this code.

### What the matching half is

`f9d8` is a sub-state under the receive state, and each value matches the
character stream against something different:

```
    0x23  wait for the outgoing sequence to drain, then swap buffers
    0x28  collect the fifteen-word message, marker 0x00f, into seq_alt
    0x29  hunt the raw bit stream for 0xc0f or 0xd55
    0x2a  hunt for CJ
    0x2c  collect and check the six-word QCA1 message into seq_spare
    0x32  stop matching (answering side)
    0x33  stop matching (calling side)
```

Two of them -- the hunt and the drain -- look at the raw shift register and
so run every block; the other four wait for ten bits to have arrived and take
`f1a & 0x3ff` as a character.

The names are the original author's, not invented. Its debug strings still
carry `QCA1a`, `QCA1d`, `U_QTS`, `LAPM Indication` and `ANSpcm level index`,
which is also what identifies the three result fields at `+0xdc4`, `+0xdc8`
and `+0xdcc` -- and those in turn are what `V8GetMessage` reads to decide
whether to hand back `seq_spare` instead of `tx_seq`.

Acceptance of a message is by repetition, not by checksum: the buffer is
armed with the marker in word 0 and all-ones in the rest, each character
either matches what is already there or replaces it, and a repetition that
ran as far as the previous one did is the message. CJ is simpler still --
nine zero bits followed by a one, twice, with the zero count in the object
because a run straddles characters.

### The comparison that had been unsigned

The transmit loop's `cmp %dx,%cx; jge` is signed, and the reconstruction had
written it `(unsigned short)f21c < (unsigned short)fa3e`. `f21c` does go
negative -- `V8Process` decrements it once per sample whatever the queue is
doing -- so the two differ in practice, and fixing the loop moved the
`V8Process` failures rather than removing them. Both changes were needed.

### And a flag that should not have been a field

Rewriting the test to construct the sub-states rather than try to reach them
put 6.8 million checks through the matching half and turned up one more
defect, two bytes at `+0xebe`, in a function that had been passing for
several commits.

`evaluateRxJMSequence` has two extension-matching loops that look identical.
The first keeps its "something has matched" flag in `febc`, a field, and
clears it before every marker word. The second keeps its flag in a stack
local, set up once before the loop -- and never cleared. The reconstruction
had made both of them fields, which meant `febe` was being zeroed on entry
to the second loop where the original leaves whatever was there, and the
flag was being reset per marker where the original carries it across. Once
anything has matched in that second loop, a later marker whose first
character is wrong is abandoned rather than scanned through.

Nothing about the source distinguishes the two. What distinguishes them is
that one is `mov %si,0xebe(%edi)` and the other is `mov %ebx,0x18(%esp)`.

### The bound check that comes after the read

At `0x78070` the collector reads `word[fdbc]` and only then checks
`fdbc <= 14`. `word[]` has fifteen entries, so `fdbc == 15` reads the CRC
field beyond it, and the matching path raises `fdbc` with no cap at all.

Measured rather than assumed: it does not fire in normal operation, because
every fifteen-word message begins with the marker and the marker resets
`fdbc` to 1. It needs a stream that never sends the marker again and whose
characters go on matching the transmit fields past the array. Reproduced as
written, but read through a view of the whole sequence object so that it
stays a defined access on this side.

## 74. Three functions nothing calls

With the matching half in, a sweep of every `T` symbol between `V8Delete` and
`CALLPROG_Create` against the reconstruction found three still missing.

`notch_filter` and `biquad_filter` are the tone detector's two filter
sections as standalone functions. Nothing in the object references either:
the compiler inlined copies into `v8_tone_detect` -- same coefficients at
`.rodata+0x5726`, same histories at `+0x24`/`+0x2a` and `+0x14`/`+0x1c` --
and left the out-of-line originals behind. They are also not identical to
what was inlined: each product is truncated to a short before it is
accumulated, which shows up as a `cwtl` after every `v8_mpyint` call and does
not appear in the inlined version. Reconstructed as their own functions
because that is what the translation unit contains, and tested against the
blob, which is the only thing that ever runs them.

`Dialer_IsDialStringInvalid` is two instructions: add 0x98 to the first
argument and tail-jump to `IsDialStringInvalid`. 0x98 is where `struct
dialer` sits inside `struct callprog`, so it is the same question asked of
the dialler the supervisor owns. Trivial, and exactly the sort of thing that
would have gone missing quietly -- which is the argument for sweeping the
symbol table rather than working from a list.

## 75. A whole negotiation, against something that has never seen this object

Everything up to here proves the reconstruction agrees with the blob, plus a
signal-layer check that our ANSam is ANSam. Neither answers the question the
work is actually for: can this thing negotiate a call with a modem that was
written by someone else, from the standard, with no knowledge of Smart Link?

It can, in both directions.

```
    SpanDSP calls, the reconstruction answers
      after 190 frames (3.8 s of audio)
      spandsp: status 2 (V.8 negotiated), call function 6, mods 0xa12
      ours   : received e0 c1 45 11 94 2a 0e ff
      ours   : agreed V.32 V.34 V.21 V.23 (V-series call)

    The reconstruction calls, SpanDSP answers
      after 137 frames (2.7 s of audio)
      spandsp: status 2 (V.8 negotiated), call function 6, mods 0xa12
      ours   : received e0 c1 45 11 94 2a 0e ff
      ours   : agreed V.32 V.34 V.21 V.23 (V-series call)
```

The audio crosses at 8000 Hz, which is what a SIP leg carries, with the
reconstructed rational resampler on our side of it because the handshake runs
at 9600. `t_spandsp_v8neg` runs both ends in one process;
`t_spandsp_v8sock` runs them as two processes with the frames going over a
datagram socket, one at a time, which is the arrangement a real call has and
which rules out anything working by accident of shared memory.

### Why it works: the framing is standard after all

The reconstruction's constants look proprietary until they are decoded. A
sequence word is a 10-bit V.21 character sent most significant bit first:
bit 9 is the start bit, bits 8 down to 1 are the octet least significant bit
first, and bit 0 is the stop bit. Undo that and the CM is ITU-T V.8:

```
    0x00f -> 0xE0   the CM/JM synchronisation octet
    0x107 -> 0xC1   tag 1, call function 6 -- V-series data call
    0x141 -> 0x05   tag 5, modulation list, first octet
    0x011 -> 0x10   a modulation continuation octet
    0x0a9 -> 0x2A   tag 0x0A, protocols, LAPM/V.42
    0x161 -> 0x0D   tag 0x0D, PSTN access
    0x1c9 -> 0x27   tag 0x07, PCM modem availability
```

Which also names the flag bytes `initTxSequence` reads, since each bit of
`cm->b0`/`b1` is one bit of one of those octets:

```
    b0 bit 3  V.90            b1 bit 0  V.22        b1 bit 4  V.23
    b0 bit 5  V.34            b1 bit 1  V.17        b1 bit 5  V.21
    b0 bit 6  V.34 half-dx    b1 bit 2  V.29        b1 bit 6  V-series call
    b0 bit 7  V.32            b1 bit 3  V.27ter
```

And it explains the CJ detector's "nine zero bits then a one": that is a
start bit, eight zero data bits and a stop bit, which is the all-zero octet
V.8 specifies for CJ.

### The one part that cannot interoperate, and it is not a defect

Setting bit 4 of `cm->b2` -- the PCM offer -- takes the handshake off the
fifteen-word CM entirely and onto the six-word exchange the object's own
debug strings call QCA1a and QCA1d, with marker `0x155` and a payload
assembled by bit-shuffling `cm->menu`. That is Smart Link's proprietary
V.90/V.92 quick-connect, not V.8, and nothing outside this object can answer
it. The interop tier leaves the bit clear and says so; the code path is
reconstructed and differentially tested against the blob like everything
else, which is the only kind of proof available for it.

### Proving the intersection, not just the exchange

Both ends offering the same four modulations does not prove either end
computed an intersection: with nothing to remove, a menu walk that never ran
leaves exactly the bits it started with and passes. So two more calls run with
the offers deliberately different.

SpanDSP calls offering V.21 and V.32 only. Our end has to drop V.34 and V.23
from what it agrees -- which exercises the decoder -- and the JM we build back
carries the narrowed list, so SpanDSP's own report comes back `0x202` instead
of `0xa12`. That is the same intersection seen from the other side of the
wire, and it is what makes `rebuildJMSequence` an assertion rather than an
assumption.

Then we call offering V.21 and V.32 only, which exercises the encoder:
SpanDSP has to see exactly those two. Narrowing SpanDSP's own parameters
would not have done it -- SpanDSP builds its JM from the CM it received
rather than from what it was configured with, so an answering SpanDSP echoes
our list back whatever it was told to offer.

Between them these cover eleven bits of the modulation list, in both
directions, and every one is a place where agreeing with the blob about a
wrong bit looks identical to agreeing about a right one.

They also found D16: the V.21 bit can never be withdrawn, because the test
that should clear it reads the stop bit as well and so is never true.

### The same call, against the original object

The socket driver exists for one reason, and it is not tidiness. The blob is
i386 and the SpanDSP built here is amd64, so the two cannot be linked into
one program -- but they can be put either side of a socket. `v8peer` is built
twice from one source, 64-bit against the reconstruction and 32-bit against
the blob's own `ref_V8Create`/`ref_V8Process`/`ref_RcFixed_*`, and
`t_spandsp_v8sock` runs all four calls against each and compares.

SpanDSP reaches the same status, reads the same call function and the same
modulation list, and takes the same number of frames to do it -- 190 when it
calls, 136 when it answers -- against the reconstruction and against the
original alike.

The peers' own verdicts are compared too, not merely printed side by side:
each answers the stop frame with a record carrying the message it decoded and
the menu it agreed, and those are asserted equal. Without that the test would
pass on two peers whose decoded messages differed, so long as the handful of
bits the expectation names happened to agree -- the two printed blocks would
differ on screen and nothing would notice. Verified by making one peer offer
a different menu on purpose: "and the same message" and "and agreed the same
menu" both fail, as they should.

That is a different question from the one the differential harness asks. That
one asks whether the reconstruction computes the same bytes as the original
for the inputs a test can construct. This one asks whether a modem written by
someone else, from the standard, negotiates the same call with each of them,
over four seconds of audio and every path the handshake takes to get there.

## 76. MEMORYC.c, which is not there

`MEMORYC.c` is translation unit 96 of 283, sitting between `DPSK.c` and
`V8Interface.c` in link order. It has been open since phase 0 because the
name promised something -- an allocator, a pool, a memory map -- and nothing
in the object obviously answered to it.

Nothing in the object answers to it because it contributed nothing.

Two pieces of evidence, and together they close it.

**No local symbols, in any section.** `ld -r` writes each input object's
locals immediately after its `STT_FILE` entry, so a TU's statics are exactly
the local symbols between its FILE entry and the next one. For `MEMORYC.c`
there are none: its FILE entry is followed directly by `V8Interface.c`'s.

```
   334: FILE  DPSK.c
   335: 000071a0  160  OBJECT  LOCAL  fsklpfcoeff600
   336: FILE  MEMORYC.c
   337: FILE  V8Interface.c
   338: FILE  V8global.c
   339: 00005420   82  OBJECT  LOCAL  ANSWER_Entrance_Filter
```

**And no `.text`.** The whole stretch from `datapumpv34` to
`Dialer_IsDialStringInvalid` is contiguous to within alignment: every gap
between adjacent functions is 15 bytes or fewer, which is what 16-byte
function alignment leaves. There is nowhere for a translation unit to hide.
At the place `MEMORYC.c` would have to be, the gap is one byte:

```
    fskdemodulate  ends 0x073e1f
    V8Create     starts 0x073e20
```

So the TU was in the link and emitted no code and no data. The most likely
reading, and the one the rest of the object supports, is that it declared
the memory interface rather than implementing it: `sysdep_malloc` and
`sysdep_free` are imported, not defined here, and they are called *directly*
-- 424 and 473 times respectively, scattered across every datapump -- with no
wrapper anywhere in between. A header-like `.c` full of macros and
declarations compiles to an empty object and leaves exactly this trace.

There is nothing to reconstruct, and `claude_re` has no file for it. That is
the finding, not a gap in the work.

### What the same evidence settled on the way past

The local symbols above also attribute two tables this reconstruction had
already recovered by reading the code. `V8Detector.c` owns

```
   346: 00005724  6  OBJECT  LOCAL  a
   347: 0000572a  6  OBJECT  LOCAL  b
```

which are the tone detector's denominator and numerator, and they are three
entries each, not two and three: `a` starts at `0x5724` with `0x4000` -- the
implicit 1.0 in Q14 -- and every reader skips it and indexes from `0x5726`.
`notch_filter` and `biquad_filter` therefore belong to `V8Detector.c`
alongside `v8_detectorinit` and `v8_tone_detect`, which is where the
reconstruction had put them for unrelated reasons.

## 77. v34handshak, and three wrong answers about its shape

`v34handshak` is 61,541 bytes -- the largest function in the object by a
factor of ten, and about a fifth of everything V.34 needs.  Planning around it
needs to know what is actually in there, and the first three attempts all
produced a confident number that was false.

The function dispatches on a state variable through three jump tables:

```
  62961: cmp $0x51,%eax ; jmp *0x2da0(,%eax,4)   82 entries, states 5..86
  62af7: cmp $0x45,%eax ; jmp *0x2ee8(,%eax,4)   70 entries, states 5..74
  64ac9: cmp $0x27,%eax ; jmp *0x3000(,%eax,4)   40 entries, states 41..80
```

Three tables over one state variable, not three separate machines: several
states appear in two of them.  `TX_L1` is dispatched at `0x62c69` from the
first and at `0x65c47` from the third, which is what a transmit switch and a
receive switch over the same state look like when the compiler has finished
with them.

> **RETRACTED, and it is the paragraph above only.**  The first two tables
> are fed by +0x3596 and the third by +0x3592: two machines, not one, and
> `rxstate` at +0x3594 has no table at all.  `TX_L1` twice is state *value*
> 51 meaning different things to different machines.  The byte counts below
> stand; what the 12,290 shared bytes ARE does not.  Finding 213.

`V34hshak.c` also carries a `StateName` table -- 87 pointers at `.data+0x6c00`
into `.rodata.str1.1`, one per state, left in the object by whatever debug
build produced it.  So the state machine is not anonymous.  The names run
`SILENCE`, `ANSAM`, `TONE_2100`, `DET_CM_JM`, `TX_PHASE1_ANS`, `SSEG`,
`SBARSEG`, `PPSEG`, `TRNSEG4`, `XMITMP`, `EXMIT`, `DATAXMIT`, and seven
`MOH_*` states for V.92 modem-on-hold.

The wrong answer, three times over, came from sizing each case as the distance
from its jump-table target to the next one.  That is wrong twice:

  - it hands every byte between the highest target and the end of the function
    to whichever case happens to sit last, which produced a 54,823-byte
    `TRNSEG4` and then, after that was noticed, a 44,883-byte `RX_PHASE1_ANS`;
  - it assumes the compiler lays each case out contiguously, and GCC does not.
    Cases are interleaved, so even the interior figures are fiction.

`tools/cfgsplit.py` answers it from the control-flow graph instead: build the
basic blocks, compute what each dispatch target can reach, and count only the
blocks reachable from that target *and no other*.  Blocks two or more targets
reach are shared machinery and belong to no single state:

```
  exclusive to one case    48958 bytes
  shared by two or more    12290 bytes
  reached by no case         162 bytes
```

with the largest single case being `DET_INFO` at 6,046 bytes -- an order of
magnitude below any of the three earlier estimates.  Grouped by the phase of
the recommendation the state names come from:

```
  tone / detect / V.8 bridge       8713 bytes   7 cases
  phase 2: INFO + line probe      22727 bytes  14 cases
  phase 3: training segments       9575 bytes   9 cases
  phase 4: MP / E exchange         4992 bytes   5 cases
  data mode + retrain               926 bytes   3 cases
  V.92 modem on hold               2025 bytes   3 cases
  shared engine                   12290 bytes
```

The shared 12 KB is not one module: its largest block is 250 bytes and there
are dozens, which is the per-sample body every state falls through into
(`dftupdate`, `dftenergy`, `detectorinit`, `V34SetINFO1aBits`, `probeselect`).
It has to be reconstructed alongside the first group of states, not before
them.

## 78. Two tools that claimed more precision than the symbol table has

Both found while sizing V.34, both the same mistake in different clothes.

`deps.py` attributed every relocation target to a translation-unit span by
address.  Sections in an `ld -r` object each start at zero, so a `.rodata`
address compared against a `.text` span map lands somewhere arbitrary: it put
`V23_IIR_FILT` in the Bell 103 bracket and the whole V.34 coefficient set in
`call.c`.  Data references are now reported separately, with no span.

`tumap.py` takes an "exact" TU's `lo` to be the address of its first local
symbol.  A TU's *globals* can precede its first local, so `lo` is an upper
bound on where the TU starts, not the boundary.  `VPcmV34Main.cpp` is anchored
at `0x9250` by `_Z14getMPrecvdBitsP12tagV34Object`, but `VPcmV34GetSNR`,
`VPcmV34GetDiagnostics` and the whole `V34SetINFO*`/`V34GiveINFO*` family sit
between `0x6f00` and `0x9250` and are plainly the same file's.  Every span
endpoint that comes from an "exact" TU is therefore soft at the low end, which
matters most for the C++ bracket -- 45% of `.text` -- where almost nothing
else is anchored.

That last point has a consequence for V.34 specifically: the INFO message
codec V.34's phase 2 cannot run without -- `V34SetINFO0aBits`,
`V34SetINFO0dBits`, `V34SetINFO1aBits`, `V34GiveINFO0dBits`,
`V34GiveINFO1aBits`, `V34GiveINFO1dBits`, `V34GiveProbeResults` -- is not in
the V.34 files at all.  It is the `extern "C"` surface of `VPcmV34Main.cpp`,
which is the V.90/V.92 side.  About 6 KB, and a prerequisite rather than a
consequence of the handshake.

## 79. V.23's rates come out exact, and Bell 103's do not

`v23FP_rx_create` configures a 3:4 multirate filter and a demodulator at 5
samples per bit. The datapump speaks 8 kHz, so:

```
8000 * 3 / 4  =  6000
6000 / 5      =  1200
```

Both steps are exact. That is worth stating because the same three modules --
`FPM_MRF_filter`, `FPM_iir_filt_II`, `FPM_FSD_demodulate` -- carry Bell 103,
where nothing lands cleanly: 8000 is converted 9:10 to 7200 so that 300 baud
comes to 24 samples per symbol, and findings 17 and 24 exist entirely because
of the arithmetic that follows from that. V.23 needed no such contrivance and
the original's author did not invent one.

The AGC block lengths are the same story from the other side. The data path's
is 30 samples and the detector's is 40; those look like unrelated constants
until the rates are applied, and then both are 5 milliseconds -- 30 at the
resampler's 6 kHz output, 40 at the 8 kHz input the detector sees. Two
different windows in samples, one window in time.

## 80. Two timeouts, in milliseconds, one of which does not fit

`v23FP_rx_progress` carries two counters, and both advance by 20 per call
rather than per sample. A call is one 160-sample block at 8 kHz, which is
20 ms, so the counters are milliseconds and the limits read as times:

| field | limit | meaning |
|---|---|---|
| `+0x23a` | `+0x23c` = 60000 | one minute to find carrier |
| `+0x236` | `+0x238`, from the configuration | how long a dead line is tolerated |

The debug strings confirm both: `Carrier Detection Time Out ` and
`Energy drop detected......`.

60000 does not fit in a signed 16-bit field, and the field is signed -- the
original loads it as `mov $0xffffea60,%eax` and stores the low word. It works
because **both comparisons are unsigned**:

```
   87003:  movzwl 0x23a(%esi),%edx
   8700a:  add    $0x14,%edx
   8700d:  cmp    %dx,0x23c(%esi)
   8701b:  jbe    8723b                 ; give up
```

Reading `0xea60` as the -5536 the field's type says it is would make
`limit <= counter` true on the first call and the receiver would give up
before it had heard anything. The reconstruction declares all four as
`unsigned short` for that reason; it is the one place in this module where
following the declared type rather than the instruction would break it.

## 81. The V.23 acquisition gate counts detections, not consecutive ones

`v23FP_rx_progress` and `DemodDataB103` are the same function with different
constants -- the same private copy for the detector, the same `+= 5` on a
state counter, the same freeze of the data gain when it reaches its target.
They differ in one instruction that is not there:

```
; DemodDataB103, on a failed detect
   ...    dsp->rx_state = 0;

; v23FP_rx_progress, on a failed detect
   87082:  jne    870a0                 ; and that is all
```

Bell 103 demands three consecutive blocks of answer tone. V.23 accepts two
blocks of 1300 Hz however far apart they fall, and once the counter has
reached 10 it never returns. The original's own debug line -- `v23 tone
detected, counter = %d,threshold = 2` -- prints the counter divided by five
against a threshold of two, so the author was counting detections and knew it.

What compensates is the detector's configuration rather than the gate. V.23
raises `FPM_TONE`'s in-band energy ratio from the shared 0.75 to 0.885
(`0x7148`), so nearly nine tenths of the energy has to be at 1300 Hz before a
block counts at all. Modulated data spends half its time at 2100 Hz and cannot
pass that test, which is what makes a two-detection gate with no reset safe.

## 82. A receiver that demodulates and then throws the bits away

Once carrier is up, `v23FP_rx_progress` charges the silence counter on any
block where the gain control reports no signal, and resets it on any block
where it does. That much is ordinary. What is not is the order of what
follows:

```
   87172:  call   FPM_FSD_demodulate    ; always
   87177:  cmpw   $0x0,0x236(%esi)      ; silence
   87189:  jne    871e9                 ; -> *nbits = 0, return 0
```

The demodulator runs first and its output is discarded afterwards, so a block
during a fade costs the work and produces nothing. The bits are not held over
either -- `FPM_FSD_demodulate` has already consumed the samples and advanced
its bit clock, so they are simply gone.

The effect is a deliberate-looking conservatism: data recovered from a signal
the gain control does not believe in is never passed up as if it were real.
The cost is the first block or two of every recovery, which at 1200 bps is
around 24 bits.

Three smaller asymmetries with `BwChDem_Progress`, the other receiver in
the same modem, written by the same hand:

- `BwChDem_Progress` sets `*nbits = 0` in its prologue. `v23FP_rx_progress`
  writes `*nbits` only on the paths that return 0, so a caller has to
  initialise it -- and the differential test poisons it before every call for
  exactly that reason.
- `BwChDem_Progress` takes its sample count as a `short`;
  `v23FP_rx_progress` takes it as an `int` and compares it as one.
- neither give-up path in `v23FP_rx_progress` stores 2 into the object's
  status field; the 2 goes straight into `%eax`. So the field holds whatever
  the last non-terminal call left, and a caller that reads it instead of the
  return value sees the receiver still claiming to be running.

## 83. The answer tone the other end cannot hear

`CreateV23Modem` builds its 2100 Hz generator from the library's shared
`FPM_TONE_CFG` and changes exactly three fields:

```
   86956:  movw   $0x834,0x20(%esp)     ; freq  = 2100 -- already 2100
   8695d:  movw   $0x0,0x24(%esp)       ; rev_period = 0
   86964:  cmpw   $0x0,(%ebx)           ; mode
   8696a:  mov    $0xee4,%eax
   8696f:  mov    %ax,0x22(%esp)        ; scale = mode ? 0xee4 : 0
```

Two of those are worth stopping on.

**`rev_period = 0`** turns off the 450 ms phase reversals that `FPM_TONE_CFG`
carries by default. The reversals are the part of V.25's answer tone that
tells a network echo canceller to disable itself. A 1200 bps FSK modem has no
echo canceller and no interest in the far end's, so V.23 sends the plain tone
-- the only place in this library that takes the shared V.25 configuration and
removes something from it.

**`scale = 0` at the terminal end** means the calling end builds a tone
generator, enters the answer-tone state, runs the generator for three seconds
and emits silence. It does not skip the state. Both ends therefore take
exactly the same time to reach data without either having to know what the
other is doing, and the terminal end's transmit buffer is filled by the same
code path in both roles. It is the same trick `TxNoCarrierB103` uses to mute
Bell 103 -- zero the modulator's scale rather than the output -- applied to a
state machine instead of a sample buffer.

## 84. Two accidents that put V.25's silence back in spec

The answer-tone sequence's two durations come from the configured sample
rate: `rate * 3` and `rate / 20`. At 8000 that is 24000 samples of tone --
three seconds, inside V.25's 2.6-to-4 -- and 400 samples of silence, which is
50 ms and is *outside* V.25's 75 +/- 20.

It ends up in range anyway. The tone state advances when `elapsed >=` its
limit and the silence state when `elapsed >`, and `elapsed` moves a whole
frame at a time:

```
   86d0f:  cmp    0x10(%ebx),%eax       ; tone
   86d12:  jl     86d27                 ; ...stay while <
   86c90:  cmp    0x14(%ebx),%eax       ; silence
   86c93:  jle    86d27                 ; ...stay while <=
```

With the 160-sample frames `dp_wrapper` delivers, 400 samples of nominal
silence takes three frames to exceed: 480 samples, 60 ms, inside the window.
A strict comparison, or a different frame size, would put it back out.

Recorded rather than fixed. It is not clear the asymmetry was deliberate --
the two states are otherwise written identically -- but the arithmetic only
works out with it, and a reconstruction that tidied the comparison would
produce a modem that answers slightly out of specification.

## 85. One comparison decides which end of a V.23 call this is

`v23_create` takes the same six arguments every datapump create does, and
uses exactly one of them to decide everything V.23-specific:

```
   4cc1:  test   %esi,%esi              ; caller
   4ccb:  sete   %dl
   4cde:  mov    %dl,0x20(%esp)         ; cfg.answer_tone
   4cdb:  movzbl %dl,%esi               ; ...and the mode argument
   4cf5:  call   CreateV23Modem
```

One `sete`, used twice. The station that did **not** place the call is the
host: it answers with the 2100 Hz tone, transmits the 1200 bps forward
channel and listens on the 75 bps backward one. The station that placed the
call is the terminal and does the opposite. Note the polarity -- `caller`
non-zero means this station originated -- which is the same inversion
`b103_create` carries.

The rest of the configuration is two constants: 8000, which is the datapump's
own rate rather than whatever the host asked for, so the answer-tone sequence
is timed against the clock the modulation actually runs on; and 700, the
carrier-loss timeout in milliseconds, which both receivers charge at 20 per
block -- 35 blocks of dead line before the call is dropped.

## 86. How many bits to ask for, when the rate is not a whole number

Bell 103 asks the modem core for six bits a block and always six. V.23
cannot: its two directions are sixteen times apart and neither is a whole
number of bits per 160-sample frame -- 24 bits one way, one or two the other.

`v23_process` does not compute it. It asks for whatever the transmitter
reported it *consumed* last block:

```
   4db1:  mov    0x18(%esi),%ecx        ; last block's answer
   4db7:  mov    %ecx,0x24(%esp)        ; ...becomes this block's request
   ...    call   V23ModemMain           ; which overwrites it with what it used
   4e68:  mov    0x24(%esp),%ecx
   4e70:  mov    %ecx,0x18(%esi)        ; ...and back again for next time
```

`V23ModemMain`'s `tx_nbits` is in/out precisely so this can work, and the
whole rate question stays inside `v23tx.c` where the bit-period table is.

The field starts at zero from the `memset`, and the fetch is gated on
`connected` as well, so the core's data is not touched until carrier has been
up once *and* the transmitter has run once. Those two together are what keeps
application data off a line that is still training -- the same job b103.c's
`tx_bits_wanted` does, reached by a different route.

## 87. `consumed` counts bits finished, not bits taken

`v23FP_tx_progress` reports how many bits it FINISHED. A bit still in flight
at the end of a call has already been read out of the caller's buffer and
parked in `held`, and it is not counted -- so the figure is one short of the
number of buffer slots read whenever a call ends mid-bit, which is almost
always.

The contract that follows is that `bits[0]` is the next bit *that has never
been handed over*, and a caller must refill from index zero with `consumed`
fresh bits. `v23_process` does exactly that:

```
   modem_get_bits(dp->modem, 1, (unsigned char *)self->tx_bits, n_tx)
```

— always into `tx_bits[0]`, never at an offset. A caller that instead keeps a
cursor into a long array and advances it by `consumed` re-supplies the held
bit, and the transmitter sends it twice: once out of `held`, once as the next
bit to start.

**How this was found, which is the point.** The forward channel hides it
completely: 160 samples is exactly eight cycles of `{ 7, 7, 6 }`, so a
datapump-sized block never ends mid-bit and `resume` is never set. The
backward channel is 320 samples per cycle, so every block ends mid-bit. Fed
through the wrong caller protocol, channel 2 came out with a 14.2% bit error
rate against SpanDSP -- and the errors were at bit indices 203, 206, 230, 236,
248, 263 ... every one of them ≡ 2 (mod 3), the third entry of the period
table, each carrying the value of the bit before it.

No differential test could have found this. The blob has the same contract and
would have been driven by the same wrong caller, and the two would have agreed
perfectly about sending the wrong bit. It took an implementation that had
never seen this object. That is the entire argument for the interop tier, and
this is the second time it has paid for itself -- the first was D4.

The reconstruction's own file comment asserted the wrong contract in so many
words ("a caller can advance its own stream by that much and the two stay in
step"), so the finding is a correction to this tree's documentation rather
than to its code: `v23_process` was right all along.

## 88. `costbl` is 255 derivable entries and one that is not

The V.34 cosine table at `.data+0x6d60` is 256 entries over one full period in
Q14, shared by `DFTC.c` and `V34RX.c`. Every entry but one is exactly

```
(short)(16384.0 * cos(2 * pi * i / 256))
```

truncated toward zero rather than rounded, which is what a generator that
assigns a double straight to a short produces. Index 128 is the exception:
`cos` is -1 there, the formula gives -16384, and the table holds **-16383**.

That is not a range limit. `-16384` is representable, index 0 carries
`+16384`, and no other entry is clamped. Nor is it a rounding artefact: the
angle at i=128 comes out as `M_PI` exactly under any evaluation order a
generator would use, and `cos(M_PI)` is exactly `-1.0` in IEEE double, so
every plausible generator gives -16384.

So one entry was floored by hand. The most likely reason is that negating a
Q14 value is only safe above -16384 and this table is read by code that
negates it -- but that is a guess, and the reconstruction does not act on it.

The consequence is concrete and small: **the table cannot be generated**, and
`src/pump/v34/dftc.c` emits it as data. A reconstruction that computed it
would be wrong by one LSB at exactly the index the phase accumulator lands on
for half a turn, which is not a rare input.

## 89. Two functions in V.34 that nothing in the object calls

`cosread` (14 bytes) is a one-line accessor returning `costbl[idx]` for a
`unsigned char` index. The relocation table has **no reference to it** from
anywhere in `.text`, and neither does any datapump's operations table. Its
own table is reached directly by the two functions that use it.

It is global, so a caller outside `dsplibs.o` could in principle reach it,
and nothing in slmodemd does. It is reproduced anyway: an exported symbol
with no internal caller is a fact about the original's interface, and this
reconstruction does not get to decide that the interface was wrong.

The same reasoning applies to `denergy`, the `double` energy `dftenergy`
writes at bin offset +0x20. Both accumulator paths are maintained on every
sample of every bin -- an x87 load, add and store each -- and no reader for
the double result has been found. Unlike `cosread` it is not exported, so
its reader would have to be inside the object; the search covered the
relocations and the constant-offset loads in `v34handshak` and `V34RX.c`, and
neither turned one up. Recorded as unresolved rather than as dead code,
because 91% of `.text` is still untranslated and a null result over the part
that is translated is not evidence.

## 90. The V.34 detector scales its two sections differently

`tone_detect` runs two second-order sections in cascade and divides by 16
between them, which is ordinary headroom management. What is not ordinary is
that the two divisions are not the same operation.

Section 1 shifts the 32-bit accumulator and then truncates:

```
   7374a:  sar    $0x4,%ecx          ; the full accumulator
   73754:  movswl %cx,%ecx           ; then take the low word
```

Section 2 truncates and then shifts:

```
   737a0:  movswl %cx,%edi           ; take the low word
   737a3:  sar    $0x4,%edi          ; then shift
```

The two agree while the accumulator fits in 16 bits and diverge as soon as it
does not -- which is precisely when the detector is being driven hard, and
therefore precisely when it is being asked to make a decision. Section 2's
order discards the high bits before they can be shifted down into range, so
its output folds instead of scaling.

There is no reading under which both are intended. One is a slip, and which
one cannot be recovered from the object: the surrounding code gives no
independent scale for either section's output, and the coefficients the
handshake installs (`obj+0xaab0`) belong to a translation unit that is still
opaque, so there is nothing to check the intended gain against.

Reproduced, and registered as D25. See the bug policy: this is not a defect
that stops a working modem, because both orders behave identically over the
levels a correctly-AGC'd detector sees, and "fixing" it would change which
signals the handshake believes it has heard.

## 91. `fsklpfcoeff600` is named for a bit rate, not a cutoff

DPSK.c's 80-tap post-detection low-pass is called `fsklpfcoeff600`, which
reads as a 600 Hz filter and is not one. Measured at the rate it actually
runs at -- 28800 Hz, the 9600 Hz input after this module's own 3x
interpolation -- its -3 dB point is about **280 Hz**, and by 600 Hz it is
already 15 dB down.

The name is the bit rate. V.34's phase 2 INFO messages are carried at
**600 bit/s**, and a 280 Hz cutoff is what a matched low-pass for 600 baud
wants. So the filter is correctly designed and correctly named; it is the
reading of the name as a frequency that is wrong, and it is the reading a
reconstruction would naturally reach for when deciding whether a table had
been transcribed correctly.

Recorded because the same trap is set several more times in this object:
`tx600c1` and `V90EchoPrefilterCoeff` sit in v34filters.c's static block
next to tables that really are named for frequencies.

## 92. The V.34 FSK slicer's counter wrap is unreachable on a live signal

`fskdemodulate` runs a phase counter that restarts when it reaches
`bit_len * 256`, and separately restarts it on every sign change of the
discriminator output. On any real FSK input the second of those fires
constantly -- a modulated signal crosses zero many times per bit -- so the
counter never gets within two orders of magnitude of its wrap.

The only input that reaches it is **silence**: a stream of zeroes never
changes sign, because the test is `< 0` on both samples and zero is not
negative, so the phase runs free. That makes the wrap exactly what it looks
like -- a backstop for a bit clock left running on a dead line -- and it
means a test that drives only signal cannot cover it. `t_v34fsk` has a
silence run for that reason and nothing else.

The same asymmetry is worth noting for its own sake: the sign test treats
zero as positive, so a signal that sits exactly at zero is indistinguishable
from one that is strongly positive as far as bit-clock recovery is concerned,
and `prev > 0` -- a *different* test, strictly greater -- is what decides the
bit value. A sample of exactly zero therefore contributes `bit_lo` while
counting as non-negative for resynchronisation. Both are reproduced.

## 93. The V.34 echo canceller's coefficients are 32 bits in two arrays

`struct v34_echo` carries two coefficient pointers, `coeff` at +0x08 and
`coeff_frac` at +0x0c, both `taps` shorts long. It is easy to read the second
as a scratch buffer. It is not.

`V34EchoAdapt` reassembles them into one 32-bit value, moves it, and splits it
again:

```
   71f70:  movswl (%ecx),%edx      ; coeff[k], SIGNED
   71f73:  movzwl (%esi),%eax      ; coeff_frac[k], UNSIGNED
   71f76:  shl    $0x10,%edx
   71f79:  add    %eax,%edx        ; one 32-bit two's-complement tap
   ...     imul %ebp,%eax ; add    ; += hist[k] * err
   71f88:  sar    $0x10,%eax
   71f8b:  mov    %ax,(%ecx)       ; high half back
   71f91:  mov    %dx,(%esi)       ; low half back
```

The signedness is the tell: high half signed, low half unsigned, which is the
only combination that makes the pair a single number rather than two.

`V34EchoFilter` reads **only the high half**, so the filter runs at 16-bit tap
resolution while the adaptation carries 32. A correction far below the
filter's own least significant bit therefore accumulates in `coeff_frac` until
it carries into `coeff` — which is how a canceller with 16-bit taps converges
to better than 16-bit accuracy, and why the LMS step can use a much smaller
effective step size than the tap width would suggest.

Nothing in the object comments on this, and a reconstruction that treated
`coeff_frac` as scratch would still pass any test whose errors were large
enough to move the high half directly. `t_v34ec` runs a closed adapt loop for
2000 iterations precisely so that taps cross zero (414 times) and carries
happen (56,836 times).

## 94. Three V.34 filters, three different rounding conventions

Worth stating together, because the reconstruction has now hit all three and
they are easy to conflate:

| function | accumulator starts at | shift | coefficients |
|---|---|---|---|
| `V34EchoFilter` | 0 | none — returns 32 bits | Q15 |
| `V34HilbertFilter` | 0 | none — returns 32 bits | Q15 |
| `V34TimingHPFilter` | 0x8000 | 16 | **Q16** |
| `fskdetect` interpolator | 0x2000 | 14 | Q14 |
| `fskdetect` low-pass | 0 | 14 | Q14 |

The echo and Hilbert filters hand back raw accumulators and V34RX.c does the
rounding, with `add $0x2000` and `sar $0xe` in both cases. `V34TimingHPFilter`
is the odd one: it rounds internally because its coefficients are Q16, so
`V34TimingHPFilterCoeff` cannot be compared like-for-like against the Hilbert
pair even though both are 16-bit tables in the same `.rodata` block.

The practical consequence is that "is this table Q15?" is not answerable from
the table. It has to come from the shift in the function that reads it.

## 95. The V.34 equaliser has two adapters that disagree about precision

`V34EqualizerAdapt` and `V34EqualizerCenterAdapt` compute the *same* complex
LMS gradient -- `dc = -e * conj(d)` in both, instruction for instruction --
over the same coefficient arrays. They differ in two ways that are easy to
miss and that make the second not a subset of the first.

**Width.** `V34EqualizerAdapt` assembles each tap from its high half and its
fractional half, exactly as the echo canceller does (finding 93):

```
   72a00:  movswl 0x0(%ebp),%edx      ; re[k], signed
   72a04:  movzwl (%esi),%eax         ; re_frac[k], unsigned
   72a07:  shl    $0x10,%edx
   72a0a:  add    %eax,%edx
```

`V34EqualizerCenterAdapt` does not read the fractional half at all:

```
   72b84:  movswl (%esi),%edx         ; re[k]
   72b87:  ...
   72b8b:  shl    $0x10,%edx          ; and nothing added
```

**Rounding.** The 32-bit adapter truncates; the centre adapter adds 0x8000
before its shift (`add $0x8000,%edx` at 0x72ba2, and the `lea 0x8000(...)` at
0x72bd1).

So over taps 36..43 the two write the same field with different arithmetic,
and the centre adapter both discards whatever the fine adapter had
accumulated below the LSB and leaves `re_frac`/`im_frac` holding a value that
no longer belongs to the tap above them. The fine adapter reads those stale
halves back on its next pass.

**The most likely reading** is that this is deliberate: a fast coarse pull on
the centre taps during acquisition, and a fine 32-bit adaptation everywhere
once the equaliser is open. Nothing in v34filters.c arbitrates between them,
so which runs when is V34RX.c's decision and is not yet reconstructed.

The reconstruction reproduces both exactly and `t_v34eq` drives them
interleaved -- the case a reconstruction that had assumed `CenterAdapt` was
`Adapt` over a sub-range would fail, and the only case where the stale halves
are actually read back.

The centre range is also the same eight taps `V34EqualizerClearCenterTaps`
zeroes, and it contains tap 40 -- the unity tap `V34EqualizerCleanUp`
installs. So the three functions agree on which taps are "the middle", which
is corroboration that 36..43 is a real boundary and not an artefact of how the
loop was compiled.

## 96. `ec_prem_coef_B3429High` does not exist, and 3429 uses one table twice

Four of V.34's five symbol rates have a pair of echo pre-emphasis tables in
v34filters.c's static block:

```
   ec_prem_coef_B2400High   ec_prem_coef_B2400
   ec_prem_coef_B2800High   ec_prem_coef_B2800
   ec_prem_coef_B3000High   ec_prem_coef_B3000
   ec_prem_coef_B3200High   ec_prem_coef_B3200
                            ec_prem_coef_B3429
```

3429 baud has no `High` variant. That asymmetry could have been a stripped
symbol, a table shared by address with a neighbour, or a rate that simply
never takes the `High` path — and it is none of those.

`V34SetupModulator` installs **`ec_prem_coef_B3429` at both call sites**:

```
   0x072e57   ec_prem_coef_B3429     <- where other rates install *High
   0x0731d9   ec_prem_coef_B3429     <- where other rates install the plain one
```

So the table is not missing and the path is not skipped. At 3429 baud the
same 84 bytes serve both roles, where every other rate has two distinct sets.

Whether that is deliberate — 3429 is the widest V.34 rate and its pre-emphasis
may genuinely not need a second variant — or an unfinished table left pointing
at its sibling, the object cannot say. It is recorded because a reconstruction
that generated the `High` tables from a rule would produce a fifth one, and
would then be wrong at exactly one rate.

## 97. The `tx*c1` shaping tables are rows of sixteen, and the last row is special

Every relocation `V34SetupModulator` makes into a `tx*c1` table is either the
base of the table or **exactly 32 bytes before its end**:

```
   tx2400c1          256 bytes      +224   =  256 - 32
   txAllPass         256 bytes      +224
   tx2800c1         1536 bytes     +1504   = 1536 - 32
   tx3000c1         1024 bytes      +992
   tx3200c1_for_v90  384 bytes      +352
```

32 bytes is sixteen shorts. So each table is a stack of 16-tap rows and the
code selects either the first or the last, which is what a set of shaping
filters indexed by some per-rate parameter looks like with the two extremes
picked out by name.

The `p*` tables are the same shape seen from the other side: every reference
to `p2400`, `p2800`, `p3000`, `p3200` and `p3429` is to `base - 32` with an
index scaled by 32 (`shl $0x5`), so those are 320-byte tables of ten 16-short
rows **indexed from one**.

This matters for the remaining work: the row size is a property of the tables,
so `V34ModulatorProcess` can be reconstructed knowing that a "filter" here is
16 taps, before its own disassembly is read.

## 98. The echo canceller's arrays are one contiguous block, and it closes two open entries

`V34InitializeImplementationSpecific` (0x71d70, 181 bytes) does nothing but
fill in the two `struct v34_echo` objects. It was one of the two functions
this reconstruction could not attribute to a translation unit; what it
installs settles both its own home and two register entries.

For the canceller at +0x80b8:

```
  +0x04 dline       obj+0x81f8      +0x18 dlen   0x678  (1656 shorts)
  +0x08 coeff       obj+0x9018      +0x1c taps   0x90   ( 144 shorts)
  +0x0c coeff_frac  obj+0x80d8
  +0x10 hist        obj+0x8ee8
```

Every one of those abuts its neighbour exactly:

```
  0x80b8  struct        0x20 bytes    -> 0x80d8
  0x80d8  coeff_frac    144 shorts    -> 0x81f8
  0x81f8  dline        1656 shorts    -> 0x8ee8
  0x8ee8  hist          144 shorts    -> 0x9008
  0x9008  (16 bytes unaccounted)      -> 0x9018
  0x9018  coeff         144 shorts    -> 0x9138  = the second canceller
```

The second canceller repeats it at +0x9138 with the same sizes and the same
16-byte gap before `coeff`, which is why the two are exactly 0x1080 apart.
Two independent readings agreeing to the byte is strong evidence the struct
is right.

**It retracts D28.** That entry called `V34EchoReportCoeff` an over-read,
because it scans `(taps/6)*6` coefficients and dumps a hardcoded 144. `taps`
IS 144, so the dump is sized to the array and `(144/6)*6` is 144 as well.
The entry was filed on the strength of two loop bounds disagreeing, without
measuring the value they disagreed about — which is precisely what the bug
policy exists to prevent.

**It bounds D27.** `V34EchoFilter`'s single wrapping subtraction suffices for
any `lag <= dlen - taps + 1 = 1513`, and a larger `lag` would be asking for a
sample older than the line holds. Dormant, with an exact figure instead of an
"unmeasured".

**And it attributes the function.** The two unattributed functions between
`V34hshak.c` and `v34filters.c` were `datapumpv34` and this one. This one
initialises v34filters.c's own object type and nothing else, so it belongs
here. `datapumpv34` is still open — it calls only `v34handshak`,
`modulatevector`, `receiver` and `v34handshakinit`, none of which are this
file's, so the balance of evidence puts it at the tail of `V34hshak.c`.

## 99. `V34TimingFilter`'s shape, before it is reconstructed

Partial reading of 0x72360-0x724c0, recorded so the next pass starts from
evidence. **Not yet reconstructed** and not yet tested against the blob.

Signature: `V34TimingFilter(struct v34_timing *t, int sample)`, where the
sample is a complex pair packed as `(im << 16) | (unsigned short)re` — the
same packing `V34TimingPrefilter` uses.

The first thing it does is shift the two input slots:

```
   72373:  mov 0x11c(%esi),%edi      ; old in0
   72379:  mov %ebp,0x11c(%esi)      ; in0 = the new sample
   72386:  mov %edi,0x120(%esi)      ; in1 = the old in0
```

which confirms `in0`/`in1` at +0x11c/+0x120 are a two-deep complex history
shared with the prefilter, and settles the direction: `in0` is newest.

Both halves are then scaled by **0x599a with rounding at 0x8000 and a shift
of 15** — 22938/32768 = 0.70001, a gain of 0.7 — before anything else
touches them.

The `iir[6][3]` field is confirmed as six three-entry arrays, addressed as
`t+0x00`, `+0x06`, `+0x0c`, `+0x12`, `+0x18` and `+0x1e`. The loop writes the
scaled input into the first two, so those are the real and imaginary input
histories and the remaining four are the two complex filters' output
histories — which matches the four `posHalfBaud`/`negHalfBaud` denominator
tables exactly.

So the function is the half-baud band-pass pair from finding/coefficients
above, run at three taps each, with a 0.7 input gain. What is **not** yet
established: the loop bound (three is inferred from the array size, not read),
what the four `V34TimingIIR_*` tables are for — nothing here reads them — and
what the function returns.

## 100. The FSK discriminator's delay line lives inside the echo canceller

Two things this reconstruction mapped independently turn out to be the same
memory.

`fskdetect` (DPSK.c) fetches a pointer from the V.34 object and treats what it
points at as a 49-entry delay line starting 0x14 bytes in:

```
   73bb3:  mov  0x80c4(%edx),%esi
   73bb9:  lea  0x14(%esi,%eax,2),%ecx
```

`V34InitializeImplementationSpecific` (v34filters.c) writes that same word:

```
   71dc0:  lea  0x80d8(%eax),%edx
   71dd2:  mov  %edx,0x80c4(%eax)
```

and `0x80c4` is `0x80b8 + 0x0c` — the **`coeff_frac` field of the first echo
canceller** (finding 98). So the pointer the FSK discriminator dereferences is
the echo canceller's fractional-coefficient array, and its delay line occupies
`obj+0x80ec` through `obj+0x8150`: 49 shorts inside a 144-short array that
spans `obj+0x80d8` to `obj+0x81f7`.

**They cannot both be live.** `V34EchoCleanUp` zeroes all 144 entries of
`coeff_frac`, which erases the whole FSK delay line, and `V34EchoAdapt`
writes every one of them each time it runs.

The reading that fits is deliberate reuse: the FSK discriminator carries the
INFO messages of phase 2, and the echo canceller's fractional coefficients
only matter once adaptation is running in data mode. Two phases that never
overlap, sharing one buffer — which is exactly the kind of economy a modem
DSP with a fixed memory budget makes, and exactly the kind that is invisible
until two modules are reconstructed separately and their maps collide.

**What this changes.** Nothing in the code: both modules address the memory
through a pointer and neither assumes exclusivity, so the reconstruction is
already faithful. What it changes is the object map. `struct v34_fskdelay` in
v34fsk.h is not a distinct object; it is a *view* onto
`struct v34_echo::coeff_frac`, and the headers now say so. When V34RX.c
arrives and defines the parent, the two must be declared as one region with
two readings — not as two members, which would double-allocate 288 bytes and
silently break the aliasing the original depends on.

It also vindicates the rule written into both headers when the second partial
map was created: extend one of the existing maps, never start a third. This
is what starting a third would have cost.

## 101. `V34TimingFilter`, second pass: the loop bound is read, and it is two loops

Continues finding 99. Still **not reconstructed** and **not tested**; three
more things are now established rather than inferred.

**The loop bound is three, read rather than guessed.** `cmpl $0x2,0x5c(%esp)`
at 0x724b2 — indices 0, 1, 2, matching the three-entry coefficient arrays.
Finding 99 inferred it from the array size, which was a weaker argument.

**It is two loops, not one.**

- **0x72407-0x724b7** walks the two shared INPUT histories — `t+0x00` real,
  `t+0x06` imaginary — against `posHalfBaud_Bcoef_*` and
  `negHalfBaud_Bcoef_*`, accumulating four sums: a real/imaginary pair per
  filter, at `0x2c/0x30(%esp)` and `0x24/0x28(%esp)`. It writes the scaled
  input into both histories as it goes — the same read-then-overwrite shift
  this module uses everywhere else.
- **0x724c1 onward** walks the *pos* filter's OUTPUT history at `t+0x0c` and
  `t+0x12` against `posHalfBaud_Acoef_*`.

**The second loop starts at index 1, not 0** — `mov $0x1,%ebp` at 0x724df.
That is what a denominator with an implicit `a0` looks like, and
`Acoef_Real[0]` is 16384, the Q14 one. So the entry is present in the table
and skipped deliberately, not missed by an off-by-one.

That last point is the one worth having in advance. A reconstruction that
started both loops at zero would multiply by 1.0 one extra time and be wrong
by exactly one tap — an error that produces a plausible-looking filter rather
than an obviously broken one, and that a casual differential test with a
short input might not separate from rounding.

So the shape is a direct-form complex biquad pair: numerator over the shared
input history, denominator over each filter's own output history, `a0`
implicit.

**Unread:** the third loop that must handle the negative filter's output
history at `t+0x18`/`t+0x1e`, what the `V34TimingIIR_*` pair is for — nothing
read so far touches it — and the return value. Disassembly beyond 0x72560 is
not yet examined.

## 102. `V34TimingFilter` inlines the high-pass with twice the gain

Third pass, reading 0x72640-0x726b0 — the function's tail. Still not
reconstructed; the middle (0x72560-0x72640, the negative filter's output
loop) remains unread.

The tail settles what the function is for. After both half-baud filters have
produced a complex output, it takes their **cross product**:

```
   7265d:  imul %eax,%ebp        ; pos_re * neg_im  (roughly)
   72663:  imul %edx,%edi
   7266d:  sub  %edi,%ebp
   7266f:  add  $0x2000,%ebp
   72675:  sar  $0xe,%ebp
```

which is the standard V.34 timing discriminator: the phase difference between
the two half-baud lines, extracted as an imaginary part, and it is exactly
what the ±baud/2 filter pair exists to produce.

That result is then run through a **40-tap filter using
`V34TimingHPFilterCoeff` over the history at `t+0x24`** — the same table and
the same array as the standalone `V34TimingHPFilter`. And it is **not the
same filter**:

| | rounding | shift | net gain |
|---|---|---|---|
| `V34TimingHPFilter` | `0x8000` | 16 | 1x |
| this inline copy | `0x4000` | 15 | **2x** |

```
   72666:  mov  $0x4000,%esi        ; vs 0x8000 at 72312
   726a6:  sar  $0xf,%esi           ; vs sar $0x10 at 72354
```

Same coefficients, same 40-tap history, same read-then-overwrite shift —
twice the output. So the two are not interchangeable, and a reconstruction
that called `V34TimingHPFilter` from `V34TimingFilter` to avoid duplicating
the loop would be wrong by a factor of two in the timing loop's gain. That is
a defect which would not show up as a wrong waveform, only as a
differently-damped timing loop, and tier-1 would catch it only if the test
drove the whole function rather than the pieces.

They also share one history array, so calling one perturbs the other. Whether
both are ever live at once is V34RX.c's question.

**Still unread:** 0x72560-0x72640, which must contain the negative filter's
output-history loop over `t+0x18`/`t+0x1e`. Until that is read the function
cannot be written; what the tail gives is the certainty that the inline
high-pass must be written out rather than delegated.

## 103. `V34TimingFilter`, fully mapped — ready to write

Fourth pass, reading 0x72560-0x72650. With findings 99, 101 and 102 this
completes the function. It is **still not written or tested**; what follows is
the structure a reconstruction should implement, and it should be checked
against the disassembly rather than trusted from here.

```
  V34TimingFilter(struct v34_timing *t, int sample)   /* (im<<16)|re */

  1. in1 = in0;  in0 = sample                      (two-deep, in0 newest)
  2. scale both halves by 0x599a, round 0x8000, shift 15   -> gain 0.7

  3. LOOP A, k = 0..2   over the shared INPUT histories t+0x00, t+0x06
     against posHalfBaud_Bcoef_* and negHalfBaud_Bcoef_*, into four
     accumulators (pos re/im, neg re/im).  Writes the scaled input into
     both histories as it goes -- read-then-overwrite.

  4. LOOP B, k = 1..2   over the POS output history t+0x0c, t+0x12
     against posHalfBaud_Acoef_*, into two accumulators.
  5. LOOP C, k = 1..2   over the NEG output history t+0x18, t+0x1e
     against negHalfBaud_Acoef_*, into two accumulators.

     Both start at 1, not 0: Acoef_Real[0] is 16384, the implicit Q14 one.

  6. Subtract each A accumulator from its B accumulator (the feedback),
     round 0x2000, shift 14, and store the four results into the [0] slots
     of the four output histories.

  7. Cross-multiply the pos and neg complex outputs and subtract, round
     0x2000, shift 14 -> the timing discriminator.

  8. Run that through 40 taps of V34TimingHPFilterCoeff over t+0x24,
     rounding 0x4000 and shifting 15 -- TWICE the gain of the standalone
     V34TimingHPFilter, which uses 0x8000 and 16 (finding 102).

  9. Return that, sign-extended from 16 bits.
```

So the whole function is: two complex band-passes at ±baud/2, their cross
product as a phase discriminator, and a high-pass on the result. That is a
textbook V.34 timing recovery loop, and every constant in it has now been
traced — the 0.7 input gain, the ±1/8-sample-rate poles
(`docs/coefficients.md`), the implicit `a0`, and the doubled high-pass gain.

**Three traps for whoever writes it**, all of which produce plausible output:

  - starting loops B and C at 0 multiplies by 1.0 an extra time;
  - delegating step 8 to `V34TimingHPFilter` halves the timing loop gain;
  - the four output histories are shifted by loops B and C but their [0]
    entries are written in step 6, *after* both loops have run — so a
    reconstruction that stores the result inside the loop corrupts its own
    feedback.

### 103a. Writing it from that map alone does not work — measured

The structure in 103 was written up as "ready to write". It is not. An
implementation following it exactly — all three loops, the index-1 starts, the
deferred `[0]` stores, the doubled-gain high-pass, every trap in 103 avoided —
was built and driven against the blob:

```
   FAIL v34 timing filter   32503/117200 checks failed
```

It was reverted, not committed. About 28% of comparisons differ, so the
skeleton is close and something inside the complex multiply-accumulates is
not: most likely the operand pairing in one of the four MACs, or which of
`iir[4]`/`iir[5]` the cross product takes, neither of which the summary in 103
pins tightly enough to reproduce.

**The lesson, which is the point of writing this down.** Finding 103 is an
accurate *description* and an insufficient *specification*. A summary of a
fixed-point DSP routine records what it does; reproducing it bit-exactly needs
the operand order, the sign of every term, and which register holds what at
each step — and those live in the disassembly, not in prose about it.

So: do not write `V34TimingFilter` from finding 103. Use 103 to know the
shape and to avoid the three traps, and take every arithmetic line from
0x72360-0x726b0 directly. The differential test is cheap and decisive — it
found this in one run — so write it incrementally and run it often rather
than writing the whole function and debugging afterwards.

The same caution applies to every "ready to write" note in this file.

### 103b. Second attempt: the cross-product sign, and where it now stands

Re-reading the tail found a definite error in 103's description. The
discriminator is

```
   7265d:  imul %eax,%ebp      ; pos_im * neg_re
   72663:  imul %edx,%edi      ; pos_re * neg_im
   7266d:  sub  %edi,%ebp      ; pos_im*neg_re - pos_re*neg_im
```

and finding 103's summary implies the opposite order — which negates the
discriminator, i.e. a timing loop that corrects the wrong way. Correcting it
took the differential result from

```
   32503/117200 failed        ->        2000/117200 failed
```

and, more usefully, **every returned value is now correct**. The remaining
failures are all `timing state`: about five bytes of the 292-byte object per
call. So the arithmetic path — both band-passes, the feedback, the
discriminator, the high-pass and its doubled gain — is right, and something
in the *history bookkeeping* is not.

Reverted again rather than committed; a function that returns the right
number while leaving the wrong state is worse than one that is absent,
because the error only surfaces after the caller has run for a while.

**Where to look next**, in order of likelihood:

  1. which index loops B and C seed their carry from — 103 says `iir[n][0]`,
     but the two loops start at k=1, so the first value written may belong at
     a different slot;
  2. whether the four `[0]` stores happen before or after the high-pass runs
     — the tail interleaves them and the order is not obvious from a summary;
  3. the 40-tap history at `+0x24`, which `V34TimingHPFilter` also writes.

Add the byte index to the test's failure message first — `diff_eq_int` prints
its `input` argument, and the current call passes `k` without a `%ld` in the
format, so the offsets are being discarded. That alone should identify which
field is wrong in one run.

## 104. `V34SetupModulator`'s stores, extracted mechanically

Groundwork for the last unreconstructed pair in v34filters.c, gathered
without reading 1,599 bytes of dispatch line by line. **Not a
reconstruction** — a map of what the function writes.

Every constant store in the function lands in one of five words at the head
of the modulator object (obj+0x1450), plus three fields further in:

```
   +0x00   0x20
   +0x04   1, 5, 7
   +0x08   3, 4, 0xe, 0x18
   +0x10   0                      (always zero, on three paths)
   +0x14   5, 6, 8, 0x15, 0x18, 0x24, 0x28, 0x31

   +0xc7c  a tx*c1 shaping row     (finding 97: base, or base + len - 32)
   +0xc8c  0xf
   +0xcb0  preemp0, or a p* row    (finding 97: 10 rows of 16, indexed from 1)
```

Five small integers per baud rate, chosen from a dispatch on
0x960/0xaf0/0xbb8/0xc80/0xd65 — 2400/2800/3000/3200/3429 — with a second
dimension the relocations show as `_for_v34` versus `_for_v90`.

The shape of those numbers is a resampler's: a fixed 0x20 at +0x00, then
small factors and tap counts at +0x04 and +0x08, and a larger figure at +0x14
that tracks the baud rate. Reconstructing the function is therefore mostly
transcription — read the dispatch, tabulate five integers and three pointers
per (baud, variant) pair, and let the differential test check the table.

That is a much cheaper job than it looked, and it is worth doing before
`V34ModulatorProcess`, which reads these fields.

**Caution carried from finding 96:** 3429 baud installs `ec_prem_coef_B3429`
in both the `*High` and the plain role. Anyone tabulating this from a pattern
will produce a fifth `High` table that does not exist.

## 105. `V34SetupModulator` dispatches on SEVEN rates, not five

Extracted mechanically from the branch targets and relocations. Corrects
finding 96/97, which assumed V.34's five symbol rates.

```
   0x258  =  600     tx600c1              ec_prem_coef_B3429
   0x960  = 2400     tx2400c1             ec_prem_coef_B2400High / B2400
   0xaf0  = 2800     tx2800c1             ec_prem_coef_B2800High / B2800
   0xbb8  = 3000     tx3000c1             ec_prem_coef_B3000High / B3000
   0xc80  = 3200     tx3200c1_for_v34     ec_prem_coef_B3200High / B3200
                     tx3200c1_for_v90  + V90EchoPrefilterCoeff
   0xd65  = 3429     tx3429c1             ec_prem_coef_B3429
   0x12c0 = 4800     txAllPass            —
```

600 and 4800 are not V.34 symbol rates. 600 baud is the rate V.34's INFO
messages are sent at — the same 600 that names `fsklpfcoeff600` (finding 91)
— so this function also configures the modulator for the phase-2 signalling
channel, not only for data. 4800 gets `txAllPass`, a flat response, which is
what a path that must not be shaped looks like.

**600 baud shares 3429's pre-emphasis table.** So `ec_prem_coef_B3429` serves
three roles: 3429's `High`, 3429's plain, and 600's. Finding 96 read the
first two as a curiosity; with 600 in the picture it looks more like a table
that was simply reused wherever a wide or unshaped response was wanted.

Per-rate integer stores at the head of the modulator object:

```
   baud    +0x00   +0x04   +0x08
    600      —       1      0x10
   2400      —       1       4
   2800      —       7      0x18
   3000      —       5      0x10
   3200      —       1       3
   3429      —       5      0xe
   4800     0x20     1       4
```

A **second, independent dispatch** then writes `+0x10 = 0` and `+0x14` = one
of 5, 6, 8, 0x10, 0x15, 0x18, 0x24, 0x28, 0x31 — nine values, so it is not
keyed on baud. It is keyed on one of the other arguments; which one is not
yet established.

GCC has interleaved the two dispatches heavily, so the branch targets do not
read in source order. Reconstructing this needs the control-flow graph, not a
linear read — `tools/cfgsplit.py` exists for exactly that and has not yet been
pointed at this function.

## 106. `V34SetupModulator` partitioned: 587 bytes of rate code, 590 of engine

`tools/cfgsplit.py` reported the whole function as "reached by no case",
which was correct and useless: it only understands jump tables, and this
function dispatches with a `cmp`/`je` chain. The tool now takes `--entries`
so a compare-chain dispatch can be partitioned too.

```
   exclusive to one case      587 bytes
   shared by two or more      590 bytes   <- the common engine
   reached by no case         412 bytes   (prologue, epilogue, padding)

     142  0x732f1   3200          83  0x731cf   3429
     101  0x72fbe   3000          40  0x72e52    600   (the default case)
      95  0x73180   2400          35  0x732a7   4800
      91  0x72dcd   2800
```

**No case exceeds 142 bytes.** A function that reads as 1,599 bytes of
interleaved dispatch is seven small blocks of per-rate constants over one
590-byte engine — which is what finding 104's store extraction suggested and
this measures.

That also settles the shape of the work: the engine is written once, and each
rate is a table row. The 600-baud case being the **default** rather than a
match is worth noting on its own — anything not recognised as one of the six
real rates gets the INFO-channel configuration.

The same `--entries` option will be needed for any other compare-chain
dispatch in what remains; `v34handshak` uses jump tables and does not need it.

## 107. `V34SetupModulator` decoded: a polyphase loader and two dispatches

The engine (0x72e7a-0x72ee8) is a **polyphase filter loader**, and it is the
same 590 bytes for every rate:

```
   for (row = 0; row < m[0x08]; row++) {
       for (i = 0; i < m[0x00]; i++)
           dst[i] = src[m[0x00] - 1 - i];      /* REVERSED */
       for (; i <= 0x3f; i++)
           dst[i] = 0;                          /* pad to 64 shorts */
       dst += 64;  src += m[0x00];
   }
```

with `dst = m[0xc24]` and `src` the `tx*c1` row the rate selected. So each
phase is stored time-reversed and zero-padded to 64 taps — which is why the
`tx*c1` tables are stacks of 16-tap rows (finding 97) and why the destination
is 64 shorts per row regardless.

**Dispatch 1, on baud** (arg1), setting three counts and two table pointers:

```
   baud   m[0x00]  m[0x04]  m[0x08]   tx*c1 source
    600      8        1      0x10     tx600c1            (the DEFAULT case)
   2400      ?        1       4       tx2400c1
   2800     0x20      7      0x18     tx2800c1
   3000      ?        5      0x10     tx3000c1
   3200      ?        1       3       tx3200c1_for_v34 / _for_v90
   3429      ?        5      0xe      tx3429c1
   4800     0x20      1       4       txAllPass
```

**Dispatch 2, on carrier frequency** (arg2), setting a sine table pointer at
`m[0x10]` and its length in COMPLEX PAIRS at `m[0x14]`:

```
   0x4b0 1200 -> hsine1200   8      0x725 1829 -> hsine1829  0x15
   0x640 1600 -> hsine1600   6      0x74b 1867 -> hsine1867  0x24
   0x690 1680 -> hsine1680  0x28    0x780 1920 -> hsine1920   5
   0x708 1800 -> hsine1800  0x10    0x7a7 1959 -> hsine1959  0x31
   0x960 2400 -> hsine2400   8      0x7d0 2000 -> hsine2000  0x18
```

Every `m[0x14]` is exactly the table's byte size / 4 — its length in complex
pairs — so the tables are interleaved (sin, cos) and this is confirmation
rather than assumption. Anything not matching falls through to `hsine1200`
after a debug message.

**The `High` / plain pre-emphasis choice is made on CARRIER FREQUENCY, not on
baud.** The 2800 case tests `0x690` (1680 Hz) and installs
`ec_prem_coef_B2800` if it matches, `ec_prem_coef_B2800High` otherwise. That
reframes findings 96 and 105 again: `High` is not a per-rate variant, it is
the not-1680-Hz variant, and 600 and 3429 having only one table means those
rates only ever run at one carrier.

Also established: `m[0xcb0] = preemp0` unconditionally at entry; `m[0xc7c]`
takes the `ec_prem_coef_*` pointer; the `p*` tables are reached as
`p<baud> + 32 * (arg3 - 1)`; and a non-zero arg5 diverts to a separate path
at 0x72f64 that zeroes `m[0x0c]` and `m[0x18]` and calls `sysdep_memset`
twice.

**Not yet written.** `m[0x00]` is unread for four of the seven rates, and the
arg5 path and the 0x73013 tail are unread. What is above is enough to write
most of it; the remainder is one more disassembly pass.

## 108. `V34ModulatorProcess`, and the buffer shift that finishes it

Written and driven against the blob: **60281 of 792000 checks failed**, all
of them output samples. `nout`, `row`, `phase` and the pre-emphasis history
all matched. Reverted under the fast-pass rule; this records how far it got.

The structure, confirmed by everything that passed:

```
   work[wpos]        = (short)symbol           /* real */
   work[wpos + step] = (short)(symbol >> 16)   /* imaginary */

   while (row < rows) {
       src = &shaped[row * 64]
       acc_re = acc_im = 0x1000
       for (i = 0; i < taps; i++) {
           acc_re += work[wpos + i]        * src[i]
           acc_im += work[wpos + step + i] * src[i]
       }
       re = acc_re >> 13;  im = acc_im >> 13

       /* up-convert; the table is sine_len sines then sine_len cosines,
          so the quadrature partner is slen entries on, not the next one */
       x = (re * sine[phase + slen] - im * sine[phase] + 0x2000) >> 14
       if (++phase >= slen) phase -= slen

       /* pre-emphasis, 16 taps, read-then-overwrite */
       out[nout++] = (acc + 0x2000) >> 14

       row += f04
   }
   m->phase = phase;  m->row = row - rows
```

**What is missing** is at 0x735ae and 0x735d4: after the row loop the work
buffer is **shifted by one**, separately for each half —

```
   735b2:  movswl 0xcbc(%edi,%edx,2),%ebx     ; read work[i]
   735ba:  mov    %bp,0xcbc(%edi,%edx,2)      ; write the carry
   735c7:  mov    %ebx,%ebp                   ; carry = the old value
```

over `taps` entries at index 0, then again at index `step`. That is a delay
line advancing one symbol per call, and without it the FIR convolves a stale
window — which is exactly the 7.6% of samples that differed.

**Unresolved:** what seeds `%ebp` on entry to that first shift. It is
`xor %ebp,%ebp` at function entry but is reused as `acc_im` inside the row
loop, so by the time the shift runs it holds something else. One more
disassembly pass over 0x73569-0x735ae settles it, and then the function is
done — everything else is already proven correct by the 731719 checks that
passed.

## 109. Where the two sample queues live, and what the echo cleaner was walking

`txinit` (0x5d670) locates both `struct v34_queue` instances in the V.34
object, and closes a loose end from finding 98's neighbourhood.

```
   obj+0x0264   the RECEIVE queue    count 0x264, rd 0x268, wr 0x26c,
                                     ring 0x270 .. 0x370
   obj+0x221c   the TRANSMIT queue   count 0x221c, rd 0x2220, wr 0x2224,
                                     ring 0x2228 .. 0x25c0
```

Both are confirmed by their own initialisation: `txinit` writes
`obj+0x270` into the receive queue's two cursors, `obj+0x2228` and
`obj+0x22a8` into the transmit queue's, and then
`sysdep_memset(obj+0x2228, 0, 0x398)` — and `0x2228 + 0x398` is `0x25c0`,
exactly the end address `txwritequeue` hardcodes relative to its own base
(`0x221c + 0x3a4`). Two independent readings agreeing to the byte.

**And it identifies the ring `V34EchoHistoryBackwardClean` walks.** That
function ends by zeroing three entries of a ring at `obj+0x2228`, backwards,
with a wrap this reconstruction described as "running off the front into a
second cursor". It is the **transmit queue's ring**, and the walk is
`&ring[(wr - 1 - k) mod len]` — the last three samples queued for
transmission, rolled back along with the echo canceller's history. Which is
exactly what a function called `EchoHistoryBackwardClean` should be doing:
if the receiver rewinds, the samples the canceller has not yet accounted for
have to go too.

That was reconstructed and differential-tested without knowing what the ring
was; it is right either way, but the comment in `v34filters.c` describing the
wrap as an oddity should be replaced with this.

`txinit` also calls `V34EchoCleanUp` on **both** cancellers, at `obj+0x80b8`
and `obj+0x9138` — the first confirmation from a caller that the pair in
finding 98 really is a pair, rather than one object and something that
happens to look like it.

**Not yet reconstructed:** `txinit` continues past 0x5d731 and the rest is
unread.

## 110. Three places the reconstruction looks wrong against the standard and is not

Collected because they share a failure mode: someone checks the code against
ITU-T V.34, sees a discrepancy, and "fixes" a function that was bit-exact.
The differential test would catch it — but only if it is re-run, and a
confident spec-based correction is exactly the change someone makes without
re-running it.

**1. The descrambler's taps read one lower than the recommendation.**
V.34 specifies `1 + x^-6 + x^-19 + ...` numbering; `V34descrambler` tests
bits 5, 18 and 23. Not an off-by-one: the register takes the input bit at
position 0 and is *then* shifted up, so every bit sits one place higher than
its tap number by the time it is tested. The polynomials are the
recommendation's, spelled against a register that has already moved.

**2. `V34EchoReportCoeff` dumps a hardcoded 144 while scanning `taps`.**
Already retracted as D28 — `taps` *is* 144 — but the retraction lives in the
deviation register and the code still contains a literal that looks
unexplained. See D28 before changing it.

**3. `decision` truncates its squared distance to 16 bits.** The sum is
shifted *logically* and then truncated, so a constellation point far enough
from the target wraps to a small distance and can win. That is a real
property of the original and is reproduced; whether it fires with a real V.34
constellation is unmeasured and belongs to task #47.

The general rule, and the reason this entry exists at all: **a discrepancy
against the recommendation is evidence about the recommendation's numbering
conventions, not automatically evidence about the code.** The blob is the
reference for this project. The standard is context.

## 111. `txinit` decoded in full — ready to write, needs one struct change first

Complete reading of 0x5d670-0x5d764. **Not written**; the blocker is
mechanical and named at the end.

```
   obj->f3550 = 0;  obj->f3552 = 0;            /* shorts               */
   obj->f25c0 = 0;  obj->f25c6 = 0;            /* shorts               */
   obj->f25cc = 0;                             /* int                  */

   V34EchoCleanUp(&obj->echo0);
   V34EchoCleanUp(&obj->echo1);

   txq->count = 0x20;                          /* 32 entries pre-queued */
   txq->rd    = obj + 0x2228;                  /* the ring's base       */
   txq->wr    = obj + 0x22a8;                  /* 32 ints further on    */
   memset(obj + 0x2228, 0, 0x398);             /* the whole ring        */

   rxq->count = 0;
   rxq->rd = rxq->wr = obj + 0x270;
   memset(obj + 0x270, 0, 0x100);              /* the whole ring        */

   memset(obj + 0x2078, 0, 0x54);              /* the prefilter's state */
```

Three things it confirms rather than assumes:

- `txq->wr` starts 32 ints past `rd` and `count` starts at 32. The transmit
  queue is **primed with 32 entries of silence**, not empty — so the
  modulator has a full block to draw on before the first symbol arrives.
- The final `memset` is 0x54 bytes at `obj+0x2078`, which is exactly
  `V34_ECHO_PREFILTER_TAPS * sizeof(short)` — 42 shorts — confirming the
  prefilter's state length from a second, independent site.
- The ring lengths match `rxreadqueue`'s and `txwritequeue`'s hardcoded ends
  a third time: `0x270 + 0x100` is `0x370` (`0x264 + 0x10c`), and
  `0x2228 + 0x398` is `0x25c0` (`0x221c + 0x3a4`).

**What is needed before it can be written:** `struct v34_object` must gain the
receive queue at `+0x264`, which currently falls inside
`unmapped_0404[0x2074 - 0x404]`, plus the three scalars at `+0x25c0`,
`+0x25c6`, `+0x25cc` and the pair at `+0x3550`. The transmit queue is already
partly there — `ring_pos` at `+0x2224` is that queue's `wr` cursor and
`ring` at `+0x2228` is its ring, which is why finding 109's identification
worked. Renaming those two into a `struct v34_queue` at `+0x221c` is the
tidy version and touches `V34EchoHistoryBackwardClean` and `t_v34fsk`.

## 112. The receive AGC's gain steps are not inverses

`agcadapt` moves its gain by one of two fixed factors when the error
integrator trips:

```
   down   0x390a / 16384  =  0.88300
   up     0x47cf / 16384  =  1.12201
```

`0.88300 * 1.12201 = 0.99074`. So a signal that oscillates about the target,
tripping the integrator equally often in each direction, **drifts downward**
by about 0.9% per pair of steps.

Whether that is deliberate — a deliberate bias toward attenuation, which is
the safe direction for a receiver that must not clip — or an artefact of
picking two round-ish Q14 constants independently, the object does not say.
It is reproduced either way.

Two other things worth having: the up-step is skipped entirely once the gain
exceeds `0x6a00`, so there is a ceiling but no floor; and the range check on
the smoothed level replaces an out-of-range value with `0x7f00` and then
**falls through to use it** rather than returning, so a single overflowing
measurement still drives one adaptation step from a fabricated level.

Reachability of both is unmeasured — `agcadapt`'s caller is `receiver`,
which is not reconstructed. Task #47.

## 113. `rxtiminginit` locates the timing object and the receive queue's output

Partial reading of 0x5b9c0-0x5ba88. **Not written** — the function is ~25
constant stores into a receiver sub-object at `obj+0x264` and writing it
needs that struct declared field by field, which is the bulk of the work.
Two placements are worth having now.

**`struct v34_timing` is at `obj+0x50c`.** The first thing `rxtiminginit`
does is `V34TimingFiltersInit(obj + 0x50c)`, so the object whose layout was
pinned in findings 99 and 101-103 — `iir[6][3]`, the 40-tap high-pass
history, the 40-entry complex prefilter state, the two coefficient pointers
— sits there.

**`rx[0x130] = obj + 0x370`**, and `obj + 0x370` is exactly
`obj + 0x264 + 0x10c` — the **receive queue's output buffer**, the four
shorts `rxreadqueue` writes immediately after its ring. So the receiver keeps
a pointer to where its samples land, and `V34_RXQ_END` is confirmed a fourth
time.

The constants stored are otherwise unremarkable except one: `rx[0x1d2]` is
set to `0x960` — **2400**, the lowest V.34 symbol rate — which is the
receiver starting up assuming the slowest rate until the handshake says
otherwise.

The remaining stores run to 0x5bac5 and are unread.

## 114. The AGC freeze and the detector-pending flag are one bit

Found by unifying four partial maps of the receiver sub-object into one
`struct v34_receiver`. Each had been declared as its own type when the
function that needed it was reconstructed — the detector's flags word,
`decision`'s target and result, `agcadapt`'s gain state,
`V34descrambler`'s shift register — which is precisely what finding 100 said
not to do, and doing it hid this.

```
   agcadapt      testb  $0x2,0x123(%ebx)      ; freeze if set
   tone_detect   andl   $0xfffffdff,%eax      ; clear 0x200 at 0x122
```

Byte `0x123` is the **high half** of the short at `0x122`, so its bit 1 is
that short's bit 9 — `0x200`. The two functions are reading and writing the
same bit.

So the arrangement is: the handshake sets bit 9 when it arms a tone detector;
`agcadapt` refuses to adapt while it is set; `tone_detect` clears it the
moment the integrated level first crosses its floor. **The AGC is held frozen
until a detector has heard something, and starts adapting on the same event
that arms the receiver.** That is a sensible design and neither function
states it — it only appears when both are looked at together.

Two functions reconstructed a long way apart, sharing a flag neither knew
about. The lesson is finding 100's, restated: partial maps of one object hide
the relationships between the things that use it, and the cost is not
confusion later but a fact never discovered at all.

`struct v34_receiver` is now the single map; `v34_rx`, `v34_decoder`,
`v34_agcstate` and `v34_scrambler` are gone.

## 115. `rxinit` stores a leftover return register into a receiver field

Full reading of 0x5ab80-0x5ad45. **Not written** — see the trap below.

Structure:

```
   rx->agc_gain = 0x200;  rx->agc_step = 0x3333;  rx->f1b8 = 1;
   V34EqualizerCleanUp(obj + 0x630);          /* the equaliser         */
   sysdep_memset(obj + 0x4ec, 1, 0xc);        /* FILL 1, NOT 0         */
   sysdep_memset(obj + 0x4f8, 1, 0x10);       /* FILL 1                */
   V34InitHilbertFilter(obj + 0xa1b8);
   rx->f218 = 0x4000;  rx->f1f2 = 0x4000;
   rx->f138 = <see below>;
   rx->agc_level = 0x4000;  rx->f1f4 = 0x4000;
   if (rx->flags & 8)  { rx->f200 = 2; rx->f202 = 10; }
   else                { rx->f1f8 = 0; rx->f200 = 2; rx->f202 = 8; }
   ... about twenty more fields cleared to zero ...
   rx->rx_samples = rx + 0x10c;
```

Two things worth having before it is written.

**The two memsets fill with 1, not 0.** `sysdep_memset(obj+0x4ec, 1, 0xc)`
writes `0x01010101`, not zeros — twelve bytes and then sixteen. Easy to
"correct" while transcribing.

**`rx->f138` is stored from `%eax` immediately after the
`V34InitHilbertFilter` call**, and that function returns nothing:

```
   5ac04:  call   V34InitHilbertFilter
   5ac09:  mov    $0x4000,%ecx
   ...
   5ac2e:  mov    %ax,0x138(%ebx)
```

`V34InitHilbertFilter` ends by tail-calling `sysdep_memset`, which returns its
destination, so `%eax` holds `obj + 0xa1b8` and its low half lands in
`f138` — the AGC's error integrator. **The value depends on where the object
was allocated.**

That makes it the first V.34 field whose correct value differs between the
blob and the reconstruction by construction: two objects at two addresses
give two answers, both "right". Writing `rxinit` therefore needs
`V34InitHilbertFilter` declared to return its argument — which is what it
does — and `t_v34rx` must skip `f138`, the same way it already skips pointer
fields.

Registered as **D34**; the field is an integrator that `agcadapt` overwrites
on its first trip, so the garbage has a short life, but it is garbage.

### 115a. `rxinit` attempted: two bytes short, and one fill value corrected

Written and driven. **60 of 87862 checks failed**, then 4 after one fix.
Reverted; recorded so the next pass starts from the measurement.

**Corrected on the way — the memsets fill with ZERO, not 1.** Finding 115
read `mov $0x1,%ecx` at the top of the function as the fill value. It is
not: `%ecx` is caller-saved and `V34EqualizerCleanUp` is called between that
load and the memsets, so what reaches them is whatever that function left
behind, which is zero. Finding 115's warning about "easy to correct while
transcribing" was itself the error. The differential test found it in one
run.

**Still failing: two bytes, at receiver +0x135 and +0x1f5.** Both are the
HIGH half of a short the disassembly sets to `0x4000`:

```
   5ac09:  mov  $0x4000,%ecx
   5ac0e:  mov  $0x4000,%edx
   5ac15:  mov  %cx,0x218(%ebx)     <- matches
   5ac1e:  mov  %dx,0x1f2(%ebx)     <- matches
   5ac27:  testb $0x8,0x122(%ebx)
   5ac2e:  mov  %ax,0x138(%ebx)
   5ac35:  mov  %cx,0x134(%ebx)     <- ours 0x4000, blob's high byte 0
   5ac3c:  mov  %dx,0x1f4(%ebx)     <- same
```

The first two stores of the same registers agree; the second two do not.
Nothing between them writes `%ecx` or `%edx`, and the `testb` sets only
flags. So either something later in `rxinit` overwrites `0x134` and `0x1f4`
and was missed in the reading, or those two fields overlap something else in
`struct v34_object` that the test is comparing.

The second is worth checking first: `struct v34_queue rxq` is declared at
`+0x264` and `struct v34_receiver` is addressed from `+0x264` too, so the
receiver's first 0x120 bytes ARE the receive queue. That overlap is real and
intended, but it means an offset error in either struct shows up as exactly
this — two isolated bytes, in fields whose neighbours are fine.

### 115b. Both hypotheses ruled out; the contradiction stands

**The struct overlap is real but harmless.** Compiling `offsetof` for every
field involved gives `flags 0x122`, `agc_level 0x134`, `f1f4 0x1f4`,
`f218 0x218`, `f1f2 0x1f2` — all exactly where the disassembly puts them —
and `struct v34_queue`'s ring at object offset `0x270`, ending at `0x370`,
well below the `0x398` in question. The structs are right.

**And nothing overwrites the two fields.** `objdump` finds exactly one store
to each in the whole function:

```
   5ac35:  mov  %cx,0x134(%ebx)
   5ac3c:  mov  %dx,0x1f4(%ebx)
```

So: `%ecx` and `%edx` are provably `0x4000`, set four and five instructions
earlier with only a `testb` and one unrelated store between; `%ebx` is
callee-saved and provably `obj + 0x264`; nothing writes either field again;
and the **first** pair of stores from those same two registers — to `0x218`
and `0x1f2`, four instructions before — lands correctly.

Static reading is exhausted and the disassembly contradicts the measurement.

**Next step is a runtime probe, not more reading.** Call `ref_rxinit` on a
known buffer and print `obj+0x398` and `obj+0x458` directly, instead of
inferring them from a byte-wise comparison. That distinguishes "the blob does
not do what the instruction says" — which would mean the reading is wrong
somewhere earlier — from "the test is comparing the wrong bytes". This
session has produced two of the latter (`V34TimingFilter`'s pointer fields,
`V34EchoHistoryBackwardClean`'s `p_2074`) and none of the former, so that is
where the prior sits.

### 115c. Resolved by the probe: my own grep filter hid two instructions

`ref_rxinit` on a known buffer printed `obj+0x398 = 0x0000` while
`obj+0x47c` and `obj+0x456` — written from the *same two registers* — both
held `0x4000`. So the blob really does write zero, and the reading was wrong.

The cause was in the tooling, not the object. Every disassembly of this
function in findings 115 and 115a was taken through
`grep -vE "89 f6|8d 76 00|^\[|xor +%e"`, and that last term removed the
register-zeroing instructions:

```
   5ac15:  mov  %cx,0x218(%ebx)
   5ac1c:  xor  %ecx,%ecx          <- filtered out
   5ac1e:  mov  %dx,0x1f2(%ebx)
   5ac25:  xor  %edx,%edx          <- filtered out
```

`0x4000` goes to `f218` and `f1f2` **only**; both registers are cleared
immediately afterwards, so `agc_level` and `f1f4` — written from them a few
instructions later — get zero.

**The tell was visible in the filtered output all along**: `0x5ac15 + 7` is
`0x5ac1c`, and the next line printed was `0x5ac1e`. A two-byte hole. Reading
addresses for continuity would have caught it without the probe, and
`tools/dis.py` exists precisely because a previous filter dropped relocations
the same way (see its header comment, and finding 78).

**Two rules earned.** Never filter a disassembly by instruction mnemonic —
filter by noise pattern only, and check that consecutive addresses abut. And
when static reading contradicts a measurement three times, run the
measurement directly instead of reading a fourth time: the probe cost one
tool call and settled what three rounds of analysis could not.

## 116. `txmit`: the transmit path in one function, and a second ring

Read to 0x5d904; the last ~50 bytes are unread. **Not written.**

```
   sym = (txq->f3b6 << 16) | txq->f3b4;          /* obj+0x25d2, +0x25d0 */
   n   = V34ModulatorProcess(obj + 0x1450, sym, local);

   for (i = 0; i < n; i++) {                     /* scale and enqueue   */
       v = (local[i] * txq->f3b8 + 0x2000) >> 14;  /* obj+0x25d4        */
       txq->count++;                             /* ONE per sample      */
       *wr = v;  ((short *)wr)[1] = 0;
       wr++;  if (wr >= ring_end) wr = ring_base;
   }

   V34EchoPreFilter(local, n, obj + 0x2078);

   if (txq->f3a7 & 2)                            /* obj+0x25c3          */
       for (i = 0; i < n; i++) {
           ... feed both echo cancellers from a ring at obj+0x35a8 ...
       }
```

Three things worth having before it is written.

**The enqueue is `txwritequeue` open-coded, with a different count step.**
`txwritequeue` adds four to `count` per call; this adds **one per sample**,
and writes the same "low half the value, high half zero" pairs into the same
ring. So the transmit queue has two producers with different accounting, and
whichever consumer reads `count` has to agree with both.

**There is a second ring at `obj+0x35a8`**, with head at `+0x35a8`, tail at
`+0x35ac`, base pointer at `+0x35b0` and length at `+0x35b4`. Its wrap is
branchless — `seta`/`neg`/`and`, i.e. `idx &= -(len > idx)` — which resets
the index to zero on overflow rather than subtracting. That is a different
wrap idiom from every other ring in this reconstruction, and it is worth not
"normalising" while transcribing.

**The echo feed is gated on `txq->f3a7 & 2`** — a byte at `obj+0x25c3`,
which is the high half of the short at `+0x25c2`. Another flag packed into a
neighbouring field, like finding 114's.

To write it, `struct v34_object` needs `+0x25d0`, `+0x25d2`, `+0x25d4`,
`+0x25c3` and the four ring words at `+0x35a8`..`+0x35b4`, and the tail from
0x5d904 needs reading.

### 116a. `txmit` attempted: correct for ~27 iterations, then drifts

Written and driven. Reverted. Two test flaws found and fixed on the way, and
one real difference left.

**Fixed: the receive queue's pointers were being compared.** `txinit` runs in
the setup and points `rxq.rd`/`rxq.wr` into each side's own object, so bytes
`0x268`-`0x26f` differ legitimately. Fifth instance this session of a
differential failure that was the test comparing addresses.

**Still failing: a difference that appears only after roughly 27 calls**, in
the region around `obj+0x20e4` — past `struct v34_echo_prefilter`'s 0x68
bytes and inside the scratch area at `+0x20e0`. Early iterations match
exactly, so the modulation, the scaling, the enqueue and the ring wrap are
all right; something accumulates.

**And a test flaw that blocked localising it**: the failure tag was encoded
as `gate * 1000000 + iteration * 20000 + offset`, but the object is 0xaba0
bytes — larger than 20000 — so the encoding is ambiguous and the reported
number does not identify a unique (iteration, offset) pair. Any future tag
must use a stride larger than `sizeof(struct v34_object)`.

**Where to look next**, given early iterations are exact: the echo feed loop
runs only when the gate bit is set, and the drift region is not one the feed
loop writes — so the suspect is `V34EchoPreFilter`'s state advancing
differently, which would mean `n` is wrong on some later call, which would
mean `V34ModulatorProcess`'s row counter is being carried differently across
calls than `txmit` expects. That is testable directly: log `n` per call from
both sides.

### 116b. The `txmit` drift was the test's own buffer overlap

Probing `ref_txmit` alone over 32 calls: `obj+0x20e4` is zero through
iteration 27 and non-zero from 28 on, while the modulator's row counter stays
0 and the queue count grows by exactly 4 per call. So the blob is behaving
consistently and something in the *setup* changes underneath it.

`obj+0x20e4` is not scratch. The modulator sits at `obj+0x1450` and its
pre-emphasis history is at `+0xc90` — `obj+0x20e0`. So `0x20e4` is
`prem_hist[2]`, and it goes non-zero exactly when the modulator starts
producing non-zero output.

**The test pointed the modulator's `shaped` bank at `obj+0x8000`.**
`V34SetupModulator` loads `rows * 64` shorts there — 512 bytes for 2400 baud,
so `0x8000`..`0x81ff`. And `V34InitializeImplementationSpecific` puts
`echo0`'s fractional coefficients at `0x80d8`..`0x81f7` and its delay line
from `0x81f8`. The three overlap. Every `V34EchoUpdateDelayLine` call in
`txmit`'s feed loop writes into the shaping bank the modulator is reading,
so the output changes once the cursor has walked far enough.

**Both sides did this identically**, which is why early iterations matched
exactly — and why it took a probe rather than a comparison to see it. Two
objects corrupting themselves the same way still diverge once the corrupted
values feed back through address-dependent state.

The fix is to the test, not the code: give `shaped` its own buffer outside
the object, as `t_v34ec` already does for the modulator tests. `txmit`
itself may well be correct as written; it has not been shown otherwise.

Worth generalising, because this reconstruction now allocates real objects in
tests: **a scratch pointer aimed inside the object under test can collide
with the object's own arrays**, and the collision is invisible to a
differential comparison because both sides suffer it equally.

## 117. `ApplyBulkDelay` cannot be tested yet, and that re-orders the work

`ApplyBulkDelay` (0x5dd10, 467 bytes) is **LOCAL** in the object, so
`objcopy` cannot give it a `ref_` alias and no test can call the blob's copy
directly. The project has handled that before — eight file-local symbols are
driven through their callers instead (see `docs/coverage.md`).

Here there is no such caller. Its only two call sites are

```
   6639f:  call 5dd10 <ApplyBulkDelay>
   66af5:  call 5dd10 <ApplyBulkDelay>
```

both inside **`v34handshak`** — 61 KB, 87 states, tasks #39-#45, not
reconstructed. There is no relocation for either, because a local symbol
called from its own translation unit needs none, which is why a relocation
search reported zero callers and a disassembly search found two.

**So it is unwritable under the fast pass**, whose one unrelaxed rule is that
nothing commits without passing a differential test. Writing it now would
produce code with no way to check it, in a tree whose entire value is that
every line has been checked.

**Consequence for the schedule.** `V34RX.c`'s remaining functions are not
uniformly available. Before starting one, check that it is either GLOBAL or
has a reconstructed caller. Of what is left in task #36:

```
   rxtiming, decoderv34, adaptecho, V34SetupDemodulator,
   V34agc, V34demodulate, modem_serrint, receiver     GLOBAL -- testable
   ApplyBulkDelay, V34demodulate?                     check before starting
```

`V34demodulate` is also LOCAL (0x5af10, 1142 bytes) and needs the same check.
`ApplyBulkDelay` should be deferred until at least one of its two calling
states in `v34handshak` exists — it is a phase 6g-6m dependency, not a 6d
one.

## 118. `rxtiming` and `V34demodulate` must be done as a pair

`rxtiming` (0x5b390, 669 bytes, GLOBAL) calls `V34demodulate` (0x5af10,
1142 bytes, LOCAL) directly:

```
   5b3f8:  call 5af10 <V34demodulate>
```

That settles the question finding 117 left open. `V34demodulate` is local and
has no `ref_` alias, but **`rxtiming` is its caller and is global**, so the
blob's copy is reachable through it — exactly the pattern the eight
file-local symbols in `docs/coverage.md` already use.

The cost is that the two cannot be separated. Writing `rxtiming` alone leaves
nothing for it to call; testing `V34demodulate` alone is impossible. They are
one unit of about 1,800 bytes, and it is the first piece of V.34's actual
receive signal path — everything before it has been initialisation, queueing
or arithmetic helpers.

What the opening of `rxtiming` already shows:

- it re-points `rx->rx_samples` at `obj+0x370` on every call, which
  `rxtiminginit` also does once — so the field is refreshed rather than
  trusted;
- the loop is driven by a counter at `rx+0x128` and a pair of positions at
  `rx+0x1ac` and `rx+0x1b0`, differenced into a step;
- after each `V34demodulate` it runs a two-tap IIR over `rx+0x208`/`+0x20a`
  with coefficients `0x599b` and `-0x3eba`, writing the result to three
  fields at once (`+0x240`, `+0x208`, `+0x244`);
- `0x599b` is one more than the `0x599a` in `V34TimingFilter` (finding 99),
  which is the sort of near-match worth not "correcting".

**Scheduling:** this is the natural next unit of task #36, and it is a
session's work rather than an increment. `V34agc` (827 B, GLOBAL),
`adaptecho` (755 B, GLOBAL) and `decoderv34` (714 B, GLOBAL) are all
independently testable and cheaper.

## 119. `decoderv34`'s opening: a four-point slicer over `rxvect4`

Read to 0x5bc88 of 714 bytes. **Not written.**

It opens by testing `rx->flags & 0x98` for equality with `0x98` — three bits
at once, and an early exit when all three are set. Then it runs the same
nearest-point search `decision` does, but **inlined with a fixed count of
four** and against a global table:

```
   5bbfc:  mov  $0x0,%ecx        <== R_386_32 rxvect4
   ...
   5bc3f:  shr  $0xe,%eax        ; the same logical shift and 16-bit
   5bc42:  cwtl                  ; truncation as decision (finding 110)
```

So `decision` and `decoderv34` carry the same distance metric — including the
truncation that lets a distant point wrap and win — written out twice. A
reconstruction that called `decision` from `decoderv34` would be tidier and
would differ if the two ever drift.

After the search it converts the winning pointer back to an index by
subtracting `rxvect4` and shifting by 2, then reads the scrambler register at
`rx+0x1a4` and a field at `rx+0x1aa`, and writes the winning point to
`rx+0x20c` — the same field `decision` writes.

**Remaining:** about 580 bytes unread, and `rxvect4` needs dumping (it is a
global; check its size before assuming four entries — the search is over four
but the table may serve several constellations).

## 120. `V34demodulate` takes its argument in a register, and dequeues one sample

Read to 0x5afe7 of 1142 bytes. **Not written.**

**It does not use the C calling convention.** The argument arrives in `%eax`:

```
   5af12:  mov  %eax,%edi        ; first instruction after the pushes
```

and `rxtiming` sets it up that way:

```
   5b3ef:  mov  %esi,%eax
   5b3f8:  call 5af10 <V34demodulate>
```

GCC does this for `static` functions when it can see every call site — the
same reason `V34demodulate` is LOCAL. It costs nothing for the
reconstruction, because the only caller is one this project also writes, and
nothing outside can observe the register choice; but it means the function
cannot be declared or called as ordinary C from a test even if it had a
`ref_` alias, which finding 118's pairing already made moot.

**Its argument is the receiver base** (`obj+0x264`) and it opens by
dequeuing **one** complex sample from the receive queue, inline:

```
   count--;                              /* (%eax)          */
   re = *(short *)rd;  im = ((short *)rd)[1];
   rd += 4;  if (rd >= base + 0x10c) rd = base + 0xc;
```

That is `rxreadqueue`'s body with a count step of **one** instead of four and
a single sample instead of four. So the receive queue, like the transmit
queue (finding 116), has two consumers that account for it differently —
`rxreadqueue` takes four and subtracts four, `V34demodulate` takes one and
subtracts one. Neither is wrong; anything reading `count` must accept both.

It then appends the real part to a buffer at `rx+0x13c` indexed by a counter
at `rx+0x19c`, resetting the counter when it passes 0x23 (35), and advances a
pointer at `rx+0x130` by two — the same field `rxtiming` re-points at
`obj+0x370` on every call.

**Remaining: about 880 bytes unread.**

### 120a. `V34demodulate`'s second block: an RMS estimator over 36 samples

Read to 0x5b0ae (about 430 of 1142 bytes).

After the dequeue, four samples are accumulated (`cmp $0x3,%ax`) and then,
once `rx->flags & 0x200` is clear — the AGC-freeze / detector-pending bit
from finding 114 — it computes the RMS of the 36-entry buffer at `rx+0x13c`:

```
   acc = 0;
   for (i = 0; i < 36; i++)                 /* counter runs 0x23 down */
       acc += ((buf[i] * 0x38e) >> 15) * buf[i];
```

`0x38e / 32768` is `0.027771`, and `1/36` is `0.027778` — so the per-sample
scale is the mean, folded into the square. The loop counts **down** from
0x23 and terminates on `inc %ax` setting ZF, which is 36 iterations, not 35.

Then a normalise-and-halve square root:

```
   shift = 0;
   while (acc <= 0x1fffffff) { acc += acc; shift++; }   /* skipped if big */
   mant = acc >> 15;
   if (shift & 1) mant >>= 1;                            /* odd exponent */
   result = (mant + 0x40) >> 7;
```

The `if (shift & 1)` is spelled as `cmp` against `(shift >> 1) * 2` rather
than a bit test, which is the same thing and is worth not simplifying while
transcribing.

So this block turns 36 samples into one RMS figure — the receiver's own
level estimate, and presumably what `agcadapt`'s `agc_input` is fed from.
That would close the loop between `V34demodulate`, `agcadapt` and the
freeze bit, but the connection is not yet read.

**Remaining: about 700 bytes.**

### 120b. `agcadapt` appears a second time, inlined inside `V34demodulate`

Read to 0x5b160 (about 590 of 1142 bytes). The block from 0x5b0d7 is
`agcadapt` written out again, instruction for instruction:

```
   5b0e7:  imul $0x6ccd,%ecx,%esi        ; the same 0.85 smoothing
   5b0f5:  shr  $0xf,%ebp                ; the same 17-bit range check
   5b0fb:  cmp  $0x1ffff,%ebp            ;   against 0 or 0x1ffff
   5b115:  testw $0x200,...              ; the same freeze bit
   5b129:  sub  $0xfa0,%eax              ; the same 4000 target
   5b134:  lea  -0x4b0(%edx),%eax        ; the same 1200 deadband
   5b146:  imul agc_step ... >> 16       ; the same integrator step
```

Every constant matches `agcadapt` (finding 112 and its deviation entries),
including the clamp to `0x7f00` on range failure at 0x5b330 and the
fall-through that uses the clamped value.

**And it closes the loop the RMS block opened.** `rx[0x12c]` is written with
the RMS accumulator and then shifted right by 16 — and `0x12c` is the int
whose *high half* is `agc_input` at `0x12e`. So the level estimate computed
in 120a is exactly what this inlined AGC consumes.

Two consequences.

**The remaining work is smaller than 1142 bytes suggests.** `agcadapt` is
already reconstructed and differential-tested (1.16M checks), so this block
can be written by calling it — but **only if it is bit-identical**, which
needs checking rather than assuming: the inlined copy might differ in a
constant, and finding 110's rule applies. If it is identical, say so and
call; if not, write it out and record the difference.

**The AGC exists twice in the object**, which is worth knowing before the
V.90 work: a fix to one would not reach the other. That is the same shape as
the `decision` / `decoderv34` duplication in finding 119, and the second
instance of the object open-coding a function it already has.

**Remaining: about 550 bytes.**

### 120c. The inlined AGC is complete and identical; and the function is two-pathed

Read to 0x5b210 (about 720 of 1142 bytes).

The `agcadapt` copy runs to its end with every remaining constant matching:
`0x1f4` for the integrator limit, `0x390a` for the gain-down step. The
gain-up arm at 0x5b369 is reached the same way. So the inlined AGC is
**complete and, so far, byte-identical to `agcadapt`** — which means the
block can be written as a call, subject to reading the two clamp arms at
0x5b330 and 0x5b34f-0x5b369 to confirm.

**Common exit at 0x5b1b0**: `rx->f12a = 0` and `rx->f12c = 0` — the sample
counter and the RMS accumulator both cleared, whatever path was taken.

**The function has two nearly identical halves.** The block at 0x5b1c6 is the
0x5af3b block again: same append to `rx+0x13c` at index `rx+0x19c`, same
`cmp $0x23`, same gain multiply against `rx+0x136` with the same
`shr $0x19` / `cmp $0x7f` range check. The difference is only which branch of
the queue-wrap test reached it — GCC has duplicated the body rather than
joining the paths.

That matters for writing it: the two halves are one piece of C, and a
reconstruction that mirrors the assembly's structure would write it twice and
have to keep both in step. Write it once with the wrap handled before it.

**Remaining: about 420 bytes** — the clamp arms, the gain-up arm, and
whatever follows 0x5b260.

### 120d. The demodulation proper: a complex down-conversion, and a signed clamp

Read to 0x5b2c0 (about 880 of 1142 bytes).

**The clamp arm at 0x5b219 is signed and branchless:**

```
   xor  %ecx,%ecx
   test %edx,%edx
   setg %cl                      ; 1 if the product was positive
   dec  %ecx                     ; 0 or -1
   and  $0xffff0200,%ecx
   add  $0x7f00,%ecx             ; 0x7f00 or 0xffff8100
```

`0xffff8100` is `-0x7f00`. So an out-of-range gain product clamps to
**plus or minus 0x7f00**, not to the rail and not to a single value —
unlike `agcadapt`'s level clamp, which is the unsigned `0x7f00` only. Two
clamps, same constant, different symmetry. It also logs when
`dsplibs_debug_level > 1`, which is how the intent is confirmed.

**The demodulation itself** (0x5b260) is a complex down-conversion against a
table the receiver holds a pointer to:

```
   ph   = rx->f1bc;             tbl = rx->f1b4;   quarter = rx->f1ba;
   sin  = tbl[ph];              cos = tbl[ph + quarter];
   I    = (re * cos + im * sin + 0x2000) >> 14;      -> rx->f240
   ... im * cos and re * sin follow, for Q ...
   ph  += rx->f1b8;                                  /* phase increment */
```

Two contiguous halves again, quarter apart — the same layout as the
modulator's `hsine*` tables (finding 116's correction), so the receiver and
transmitter share a table convention even though the tables themselves are
separate.

And `rx->f240` is the field `rxtiming` reads immediately after calling this
function (finding 118). So the chain is now traced end to end:
**dequeue -> gain -> RMS -> inlined AGC -> down-convert -> `f240` ->
`rxtiming`'s IIR.**

**Remaining: about 260 bytes.**

### 120e. Fully read, and the inlined AGC is confirmed byte-identical

All 1142 bytes. The tail settles both open questions.

**The inlined `agcadapt` is identical in full.** Every remaining arm matches:

```
   5b330:  mov $0x7f00,%bx ; mov %bx,0x134   ; the unsigned level clamp
   5b341:  neg / sub $0x4b0                  ; |err| - deadband
   5b34f:  mov %cx,0x138                     ; store the integrator
   5b35b:  neg / sub $0x1f4                  ; |acc| - limit
   5b369:  cmp $0x6a00 ... imul $0x47cf      ; the gain ceiling and up-step
```

`0x6a00`, `0x47cf`, `0x390a`, `0x4b0`, `0x1f4`, `0x7f00` — all of them.
So the block **can be written as a call to `agcadapt`**, which removes about
a fifth of the function and reuses 1.16M checks of existing coverage.
Finding 120b said to verify rather than assume; verified.

**The complex output and the phase wrap:**

```
   I  = (re * cos + im * sin + 0x2000) >> 14;   -> rx->f240
   Q  = (im * cos - re * sin + 0x2000) >> 14;   -> rx->f242
   ph += rx->f1b8;
   if (ph >= rx->f1ba) ph -= rx->f1ba;          /* wraps at the quarter */
   rx->f1bc = ph;
```

The phase wraps at `f1ba`, the same value used as the sine/cosine offset —
confirming the table is `f1ba` sines followed by `f1ba` cosines, identical in
shape to the modulator's (finding 116) and to `V34SetupModulator`'s
`sine_len`.

**`V34demodulate` is fully specified.** What remains before it can be
written is `rxtiming`'s own body (about 540 bytes, of which ~130 are read in
finding 118), plus the struct fields: `f13c[36]`, `f19c`, `f1b4`, `f1b8`,
`f1ba`, `f1bc`, `f240`, `f242` — several already present.

## 121. `rxtiming`'s loop, and a third resonator at an eighth of the sample rate

Read 0x5b46a-0x5b546, which with finding 118 covers the loop body.

Per iteration: call `V34demodulate`, then run a **two-pole IIR** over each of
its outputs:

```
   I' = ((f240 << 10) + f208 * 0x599b + f20c * -0x3eba) >> 14;
   f20c = f208;   f240 = f208 = I';
   Q' = ((f242 << 10) + f20a * 0x599b + f20e * -0x3eba) >> 14;
   f20e = f20a;   f242 = f20a = Q';
```

so `f208`/`f20c` and `f20a`/`f20e` are the two state words of each filter,
and the input is shifted left 10 before entering — a gain of 2^10 against a
Q14 denominator, i.e. 1/16.

**Its poles are at 45 degrees.** Normalising, the denominator is
`1 - 1.4002 z^-1 + 0.9800 z^-2`: radius `0.98995`, angle `45.0` degrees —
**an eighth of the sample rate**, exactly like the half-baud band-pass pair
in `docs/coefficients.md`. That is the third structure in V.34 tuned to
`baud/2` at four samples per symbol, after `posHalfBaud`/`negHalfBaud` and
the fourth-order `V34TimingIIR` pair.

`0x599b` here is one more than `V34TimingFilter`'s `0x599a` (finding 118
flagged the near-match). With the pole angle now computed, the two are the
same design at one LSB of difference — almost certainly independent roundings
of the same real number, and **not** to be unified.

The loop is bounded by `f128` and indexed by a counter, with a step derived
from `f1b0 - f1ac` — the position pair finding 118 identified.

**Remaining in `rxtiming`: the entry and exit blocks**, 0x5b546-0x5b62d,
about 230 bytes.

### 121a. `rxtiming` complete: a fractional resampler with a magnitude detector

The block at 0x5b546 and the loop control at 0x5b5a8 finish it.

**Per output sample**, it linearly interpolates between two consecutive
demodulator outputs and takes the squared magnitude:

```
   I = (f240 * wa + f244 * wb + 0x2000) >> 14;
   Q = (f242 * wa + f246 * wb + 0x2000) >> 14;
   m = ((short)I * (short)I + ... + 0x2000) >> 14;   /* |z|^2, twice-rounded */
   out[i] = V34TimingHPFilter(timing, (short)m);      /* into rx+0x27a */
```

Each square is truncated to 16 bits before the sum, and the sum is rounded
and shifted again — so the magnitude is computed in Q14 throughout with two
separate roundings, not one.

**The resampling** is the loop control:

```
   pos += rx->f1ae;                        /* fractional step  */
   if (pos >= rx->f1b0) {                  /* crossed a sample */
       pos -= step;  ... V34demodulate(rx);   /* pull a new one */
   }
   rx->f1ac = pos;
   if (++i >= rx->f128) break;
```

So `f1ac` is a phase accumulator, `f1ae` its increment and `f1b0` its
wrap — and `V34demodulate` is called **only when the accumulator crosses**,
not once per output. That is why the two could never be separated: the
consumption rate of the receive queue is decided here, not there, and it is
fractional.

**Both functions are now fully read.** The pair is:

```
   rxtiming: for each of f128 outputs
       advance a fractional phase; on crossing, V34demodulate() ->
           dequeue one sample, gain, RMS, inlined agcadapt, down-convert
       two-pole IIR at baud/2 over I and Q
       interpolate, square, V34TimingHPFilter, store to rx+0x27a
```

Nothing further needs reading before writing them.

### 121b. Both written; the struct landed, the pair segfaults

`struct v34_receiver` was extended for both functions and **that part is
committed and green** — `rms_buf[36]` at `+0x13c`, `f19c`, the fractional
phase trio `f1ac`/`f1ae`/`f1b0`, the carrier pointer at `+0x1b4` with
`f1b8`/`f1ba`/`f1bc`, `f240`-`f246`, and `timing_out[64]` at `+0x27a`. All
12 existing test groups still pass through it.

Both functions were then written from findings 118-121a and compile clean,
but the test **segfaults**. Reverted; the struct kept.

**Most likely cause, in order.** The test primes the queue with
`oa.rxq.ring[b]` for `b` up to 63, and `struct v34_queue` declares
`ring[1]` with the remainder in the enclosing object's `rxq_ring_tail`. That
is the same out-of-bounds-in-C-but-fine-in-layout pattern that D26's clearing
loop had, and at `-O2` it is not safe to assume it behaves. Writing through
a `short *` derived from `&rxq` rather than indexing `ring[]` would settle it.

Second candidate: `V34demodulate` pulls one sample per phase crossing, and
with `f1ae = 300`, `f1b0 = 1024` and 12 outputs per call it crosses about
four times — 100 pulls over 25 calls against a 64-entry ring. The ring wraps,
so no read leaves the buffer, but `count` goes deeply negative and nothing
checks it. Whether the blob guards that is unread.

**What is proven:** the struct extension is correct and harmless. **What is
not:** either function. Neither has passed a single differential check, and
finding 121a's claim that "nothing further needs reading" is now known to be
optimistic — the crash is in code that needed no further *reading*, but did
need a test fixture that respects C's aliasing rules.

### 121c. Fixture fixed; two real errors in `V34demodulate` located

The segfault was the test: it called `rxinit` and `rxtiminginit` but not
`txinit`, and **`txinit` is what sets the receive queue's cursors**. A
receive-only fixture leaves `rxq.rd` NULL. Setting the cursors explicitly
fixes it — and the ring must still be reached through a derived pointer, not
`rxq.ring[]`.

With that, the pair runs and fails **651 of 876720 checks (0.07%)**. The
failing offsets are `rx+0x10c`, `+0x12a`, `+0x12c`-`0x12f`, `+0x138` and
`+0x208`, which name two errors precisely.

**1. `V34demodulate` writes its gained samples through `rx_samples`.**

```
   5af5f:  mov  0x130(%edi),%esi     ; rx_samples
   5afa0:  lea  0x2(%esi),%edx
   5afa3:  mov  %edx,0x130(%edi)     ; advance it
   5afa9:  mov  %cx,(%esi)           ; store the gained sample
```

Every gained half is stored there and the pointer advances by two. The
reconstruction dropped it entirely, which is why `rx+0x10c` differs.

**2. `f12a` and `f12c` are not cleared unconditionally.** When the sample
count is still `<= 3` the function stores the *incremented* count and the
running accumulator (0x5b253) and returns; only the path that runs the AGC
clears them at 0x5b1b0.

**3. And `f12c` is not what finding 120a said.** It is a running sum of the
gained imaginary part squared — `imul %ebp,%ebp; add %esi,%ebp` at
0x5afed — whose *high 16 bits* become `agc_input`. The 36-entry `rms_buf` is
a separate path that ends in a **table lookup at `.rodata+0x2860`** indexed
by the normalised accumulator and shifted by the exponent (0x5b0bf), not the
shift-and-halve square root finding 120a described.

So finding 120a is **wrong about which quantity feeds the AGC**.

### The table is `sqrt_table`, and it is exactly that

`.rodata+0x2860` is `sqrt_table`, 384 bytes — **192 entries, Q15 in and
Q15 out**:

```
   sqrt_table[i] == round(sqrt((i + 0x40) * 128 / 32768) * 32768)
```

to within **1 LSB across all 192 entries**. The first is 16384 (`sqrt(0.25)`
in Q15) and the last 32703, just under full scale.

The full sequence, then, is a normalise-index-shift square root:

```
   while (acc <= 0x1fffffff) { acc += acc; shift++; }
   mant  = acc >> 15;
   if (shift & 1) mant >>= 1;              /* odd exponent  */
   idx   = ((mant + 0x40) >> 7) - 0x40;
   if (idx > 0xbf) idx = 0xbf;             /* 192 entries   */
   result = sqrt_table[idx] >> (shift / 2);
```

which is what finding 120a described up to the halving and then invented.
The table is the missing step, and its index range `0x40`..`0xff` maps
mantissas in `[0.25, 1)` — the range a normalise-to-bit-30 leaves after the
odd-exponent halving.

**It is the first V.34 table this reconstruction has found that is fully
derivable**, so unlike `costbl` (finding 88) it need not be shipped as data —
though it should be, for the same reason every other table is: the generator
is a claim, the bytes are the reference.

### 121d. Four fixes applied: 651 -> 576, and the remainder is the level gate

All four fixes from 121c were written and each one moved the number:

```
   651  starting point
   600  gained real part written through rx_samples, pointer advanced
        f12a/f12c not cleared on the count <= 3 path
   576  the accumulator squares the REAL part, not the imaginary
        (0x10(%esp) at 0x5afe3 is where the real half was saved)
```

Reverted, since it still fails. **The remaining 576 are localised**: the
failing offsets are now only `rx+0x138` (`agc_accum`) and `rx+0x208` (the
IIR state), and the pattern is ours non-zero where the blob has zero.

That means **the blob is not running the AGC where the reconstruction is** —
so the level gate differs, not the AGC itself. The gate is

```
   5b0cd:  cmp $0x1f,%ax
   5b0d1:  jle 5b1b0          ; skip the AGC entirely
```

with `%ax` the `sqrt_table` result shifted by half the exponent. Ours comes
out above 0x1f where the blob's does not, so the fault is in the level
computation feeding it — most likely the `shift / 2` (the original uses `%cl`
set somewhere not yet traced) or the sign handling of `lvl`, which is read as
a signed short after an unsigned table load.

**Next step, and it is one probe:** log `lvl` per call from both sides.
Every previous localisation in this function came from a probe rather than a
re-read, and this one is a single value.

### 121e. The probe: the blob's AGC trips every time, so `agc_accum` is always 0

Driving `ref_rxtiming` alone and printing the receiver's AGC state per call:

```
   it | f12a  acc32       agc_input  agc_accum  agc_level
    0 |   1   0x0134fd90    308          0          0
    1 |   2   0x023a8520    570          0          0
    2 |   3   0x0314a8b9    788          0          0
    3 |   0   0x00000000      0          0        967
    4 |   1   0x00b3d511    179          0        967
    5 |   3   0x01a9397e    425          0        967
    6 |   0   0x00000000      0          0       1321
```

Three things it settles.

**The AGC runs when `f12a` reaches 4 and then clears both fields** — the
`1,2,3,0` pattern — confirming the `<= 3` short path and the clear on the
long one. The step from 1 to 3 between calls 4 and 5 is the fractional
resampler pulling two samples in one `rxtiming`, as expected.

**`agc_accum` is zero because the AGC trips, not because it is skipped.**
`agc_level` reaches only 967 against a 4000 target, so the error is -3033,
well past the 1200 deadband; the integrator step is
`(0x3333 * -3033) >> 16 = -607`, past the 500 limit, so it trips and resets
to zero on every single run. The level gate is therefore **not** the
problem — finding 121d's conclusion was wrong.

**So the fault is that the reconstruction's AGC does NOT trip**, meaning its
`agc_level` or `agc_step` differs by the time `agcadapt` is called. Since
`agc_level` at `rx+0x134` does not appear in the failing offsets, the suspect
is narrower still: either the level differs only transiently, within a call,
or `agc_step` is being read before `rxinit` set it.

**Next:** the same probe on both sides in one run, printing `agc_level` and
`agc_step` immediately before each `agcadapt` entry. That is the comparison
121d should have been.

### 121f. Both sides start identical; the divergence is in the fourth sample

`agc_step`, `agc_gain` and `agc_level` after `rxinit` are **13107, 512 and 0
on both sides**. So finding 121e's second suspect — `agc_step` read before
`rxinit` set it — is ruled out, and the divergence is behavioural.

The reference's trajectory, with the gate opened:

```
   f12a=1  agc_input=308   level=0     gain=512
   f12a=2  agc_input=570   level=0     gain=512
   f12a=3  agc_input=788   level=0     gain=512
   f12a=0  agc_input=0     level=967   gain=574   <- the AGC ran
```

`agc_input` is the high half of the running `acc32`, and it accumulates
across the short-path calls. At `f12a=3` the accumulator is `0x0314a8b9`,
whose high half is 788 — but the level the AGC computes is **967**, not 788.

**So the fourth sample is added before the AGC runs, not after.** The order
is `acc32 += re*re; f12a++; if (f12a > 3) { run AGC; clear }` — the
accumulator takes the fourth sample and *then* the AGC consumes it. A
reconstruction that ran the AGC first, or that cleared before accumulating,
would be one sample light and produce a level below the trip threshold —
which is exactly the observed symptom, the reconstruction's integrator not
tripping.

`512 * 0x47cf >> 14` is 574, so the gain-up arm is confirmed live and the
constants in finding 112 are right.

**This is the last unknown.** The ordering above, plus the four fixes in
121d, should close the remaining 576. Nothing further needs probing.

### 121g. The ordering was already right; our AGC never runs at all

Applying the ordering from 121f changed nothing — still 576, same offsets.
But the *values* localise it exactly.

`rx+0x138` (`agc_accum`) reads `248, 101` on our side, which little-endian is
`0x65f8` — **D34's seed**, the low half of `&object + 0xa1b8` that `rxinit`
leaves there. The blob reads 0, because its AGC ran and the integrator
tripped.

So the reconstruction's AGC is **never entered**, and `agc_accum` still holds
its initial garbage. That is not an ordering fault and not an arithmetic one:
`agcadapt` is proven correct by 7.1M checks, and if it had run with any
plausible level it would have tripped and zeroed the field.

**So 121e's retraction of 121d was itself wrong**, and this is the third
reversal on the same question. The record is worth stating plainly:

  - 121d: "the blob is not running the AGC" — wrong, it runs every time
  - 121e: "the level gate is not the problem" — wrong, the gate is exactly it
  - 121g: our gate never opens, and `agc_accum` proves it by retaining D34

The lesson is the one 121e already drew and then mis-applied: **do not infer
which side did less work from which side has zeroes.** Here the zero is the
side that *did* the work, and the non-zero is untouched initial state. Both
readings are available from the same two numbers, and only the provenance of
the value — `0x65f8` being an address — distinguishes them.

**Next, and it is a one-line probe:** print `lvl` and `f12a` from inside our
`V34demodulate`. Either `f12a` never reaches 4, or `lvl` never exceeds 0x1f.
Two candidates, one print, no more inference.

### 121h. Probed our own side: 576 -> 320, and the last gap is a pre-loop block

The probe answered it in one run, as 121g predicted it would:

```
   OURS f12a=4 in=967 lvl=2660 shift=7
```

`agc_input` is **967 — identical to the blob's** — and `lvl` is 2660, far
above the `0x1f` gate. So our AGC does run, and 121g's conclusion that it
never does was wrong too. Four inferences, four reversals, one probe.

What the failing bytes actually were, in order as each was fixed:

  - **`agc_accum` (`rx+0x138`)**: D34's address-derived seed, which differs
    between the two objects until the AGC first runs. The test had to skip
    it, exactly as `t_v34rx`'s `rxinit` case already does. **576 -> 570.**
    The *sixth* instance this session of a differential failure that was the
    test comparing something that cannot match.
  - **`f208` (`rx+0x208`)**: the two-pole IIR was omitted from `rxtiming`
    altogether — findings 118 and 121 described it and the reconstruction
    simply did not have it. Adding it, with the two halves of
    `decision_point` as each filter's second state word, took **570 -> 320.**

**What remains, identified:** `rx+0x244` and `rx+0x246`. The object writes
the IIR result to **three** fields in its pre-loop block —

```
   5b443: f240 = cx    5b44a: f208 = cx    5b451: f244 = cx
   5b47c: f242 = dx    5b483: f20a = dx    5b48a: f246 = dx
```

— and to only **two** inside the loop. So `f244`/`f246` are seeded once,
before the loop, and then left as the interpolator's "previous" pair. The
reconstruction has no pre-loop block at all.

That is the last gap. Adding a pre-loop IIR pass that also writes
`f244`/`f246` should close the remaining 320.

### 121i. The loop is inverted, and each crossing demodulates TWICE

Reading the entry path settles the last gap, and the reconstruction's whole
loop shape is wrong.

```
   5b3d6:  jmp 5b546            ; enter at the OUTPUT block, not the top
   ...
   5b5cf:  jge 5b3e0            ; and branch back only when the phase crosses
```

So `rxtiming` **starts** at the interpolate-and-output block and jumps back to
the sample-pulling block only when the fractional phase crosses. The
reconstruction has it the other way round — test the phase, maybe pull, then
output.

And the pulling block pulls **twice**:

```
   5b3f8:  call V34demodulate      ; first sample
   5b3fd:  ... IIR, writing f240, f208 AND f244
   5b491:  call V34demodulate      ; second sample
   5b498:  ... IIR, writing f240, f208 only
```

That is the whole explanation for `f244`/`f246`. They are not a pre-loop
seed: **they are the previous sample's filtered output**, written by the
first of the two IIR passes, while the second overwrites `f240`/`f242` with
the current one. The interpolator then has a consecutive pair to work
between — which is what an interpolator needs and what the reconstruction
was never giving it.

So the correct shape is:

```
   goto output;
   pull:
       V34demodulate();  IIR -> f244, f246 (via f240/f208, then copied)
       V34demodulate();  IIR -> f240, f242
   output:
       interpolate between the pair, square, high-pass, store
       advance the phase; if it crossed, goto pull
```

**This is a restructure, not an addition.** The 320 remaining failures are
one symptom of it; the fact that the reconstruction passed 876360 of 876680
checks with the wrong loop shape is a caution about how much a differential
test can agree with while still being wrong about structure.

### 121j. The restructure made it worse, and the evidence conflicts

Applying 121i took the failures from **320 to 1141**, and the first failing
offset is `rx+0x00` — the queue count, off by one. So the restructured loop
pulls a different number of samples, which is the one thing 121i claimed to
fix.

**The two readings contradict each other.**

The disassembly says two pulls per crossing: 0x5b3e0 calls `V34demodulate`,
runs an IIR writing three fields, falls through to 0x5b491, calls
`V34demodulate` again, and runs an IIR writing two.

The probe (121e) says one. `f12a` — which `V34demodulate` increments on every
call — reads `1, 2, 3, 0` across four consecutive `rxtiming` calls. Two pulls
per crossing would step it by two.

Both cannot be right, and the probe is the measurement. So the fall-through
at 0x5b48a to 0x5b491 is probably not a fall-through: either the block at
0x5b3e0 ends in a jump this reading missed, or 0x5b491 is a separate branch
target reached only on a path the probe's parameters never take.

**Reverted to the 320 state's shape** — which is wrong about `f244`/`f246`
but right about the pull count, and therefore closer.

**Next, and it must be a control-flow answer rather than another guess:**
run `tools/cfgsplit.py --entries` over `rxtiming` with 0x5b3e0, 0x5b491 and
0x5b546 as entries. That tool exists precisely to say which blocks reach
which, it was extended for compare-chain dispatch in this session, and it
would settle the fall-through question directly instead of by inference.

Five reversals on this function now. Every one came from reading control flow
by eye; every correction came from a measurement.

### 121k. cfgsplit settles it: the pull block is a multi-crossing loop

```
   rxtiming: 669 bytes, 10 blocks, 3 entries
     exclusive to one case      0 bytes
     shared by two or more    588 bytes
     0x5b546 output   7 blocks      0x5b491 second   7 blocks
     0x5b3e0 pull     7 blocks
```

**Nothing is exclusive and every entry reaches all seven blocks.** So the
three are one strongly-connected loop, and 121i's reading of a linear
fall-through — output, then pull, then pull again — was the wrong shape.

Re-reading `0x5b3e0` with that in hand:

```
   5b3e0:  sub  %ebx,%edx           ; phase -= step
   5b3e5:  cmp  %ecx,%eax
   5b3e7:  jl   5b605               ; ONE crossing: take the short tail
   5b3ed:  sub  %ebx,%edx           ; otherwise subtract AGAIN
   5b3f8:  call V34demodulate       ; and pull another
```

So the block is not "pull twice". It is a **loop over however many times the
phase has wrapped**, with `0x5b605` as the single-crossing exit. The two
`V34demodulate` calls are the same call site reached on different iterations,
which is why cfgsplit shows them mutually reachable and why `f12a` stepped by
one in the probe — the probe's parameters never produced a double crossing.

**So the correct shape is a `while`, not an `if`:**

```
   output;
   while (phase has crossed) { phase -= step; V34demodulate(); IIR; }
```

and the reconstruction's `if` is right for every input that crosses at most
once — which is why it reached 320 of 876680 and why the failures were
confined to `f244`/`f246`, the fields the second iteration would write.

This is the fifth reversal on `rxtiming` and the first one settled by a tool
rather than by eye. cfgsplit answered in one run what five readings did not,
and it was extended for compare-chain dispatch earlier in this same session
without my thinking to point it at a loop.

### 121l. Not a `while` either: exactly one or two pulls, on two distinct paths

Applying 121k's `while` gives 2238 of 2630040 across a five-step sweep
(0.085%), with the failure now in `agc_gain` rather than `f244`/`f246`. So
the copy is placed right and the pull *count* is still wrong.

Tracing the two paths precisely:

```
   5b3e0:  phase -= step
   5b3e7:  jl 5b605                  ; ONE wrap
                 5b605: store phase; f244 = f240; f246 = f242
                 5b628: jmp 5b491    ; then ONE demodulate + IIR
   5b3ed:  phase -= step             ; TWO wraps
   5b3f8:  V34demodulate; IIR writing f240, f208, f244
   5b491:  V34demodulate; IIR writing f240, f208
```

So it is **not** an unbounded loop. There are exactly two paths:

  - **one wrap:** copy `f240`/`f242` down to `f244`/`f246`, then one pull.
  - **two wraps:** subtract twice, then *two* pulls — the first writing
    `f244` from its own IIR result, the second writing `f240`.

Both converge on `0x5b491`, which is why cfgsplit reported them mutually
reachable and why 121k read it as a loop. It is a loop in the graph and a
two-way branch in the source.

Three or more wraps are not handled at all: the phase would still be above
`f1b0` after two subtractions and the function would carry the excess. With
`f1ae < f1b0` that cannot happen, so it is a bound on the caller rather than
a defect — worth confirming when `receiver` is reconstructed.

**The correct shape:**

```
   output;
   pos = phase + step;
   if (pos >= wrap) {
       pos -= wrap;
       if (pos < wrap) { f244 = f240; f246 = f242; pull(); }
       else            { pos -= wrap; pull(); pull(); }
   }
   phase = pos;
```

Sixth reading of this control flow. The tool said "one loop" and that was
true of the graph; the source distinction is which of two entries into that
loop is taken.

### 122. A test that skips a byte and asserts your own reading is not a test

`rxinit` writes `agc_accum` from `%ax`.  I read `%eax` as holding
`V34InitHilbertFilter`'s return value, filed it as deviation D34 -- "seeds
the AGC integrator from a stale return register" -- and then wrote the
`rxinit` differential test to **skip that byte** and assert instead that the
field equals the Hilbert buffer's address.

The assertion passed, every run, for as long as it existed.  It could not
have done anything else: it compared the reconstruction against the belief
that produced the reconstruction.  The blob was not consulted at either end.

What the blob actually does:

```
   5ac13:  xor  %eax,%eax           <-- before the stores, not between them
   5ac15:  mov  %cx,0x218(%ebx)
   5ac1c:  xor  %ecx,%ecx
   5ac1e:  mov  %dx,0x1f2(%ebx)
   5ac25:  xor  %edx,%edx
   5ac2e:  mov  %ax,0x138(%ebx)     <-- agc_accum = 0
```

All three registers are loaded, used once, and cleared.  I traced that
correctly for `%ecx` and `%edx` -- the source comment even said so -- and
attributed `%eax` to the call, because its `xor` sits *before* the first
store rather than between two of them.

It was caught by the `rxtiming` test, which drives `rxinit` from a different
starting state and disagreed by exactly this field, 29688 against 0.

**The rule.** A differential test compares against the reference.  The moment
a field is excluded from that comparison and replaced by an assertion about
what it should be, it stops being evidence and becomes an echo.  There are
legitimate exclusions -- pointers hold each side's own addresses -- but the
test for one is "no comparison is *possible* here", not "I know what this
should be".  Two of the three exclusions in that fixture were pointers; this
one was a theory.

Sibling of D28, which was also filed on a reading rather than a measurement,
and is also retracted.  Both were bug *claims*, which is the category where
this is most dangerous: a claimed defect that is really a reconstruction
error will be faithfully preserved forever, because the deviations register
exists to stop anyone "fixing" it.

### 123. The receive burst has fourteen shorts of headroom, then eats the loop counter

`rxtiming` points `rx_samples` at `+0x10c` and `V34demodulate` appends one
gained sample per pull.  What follows `+0x10c` is not spare buffer -- it is
the receiver's own bookkeeping:

```
   +0x10c   the burst grows from here
   +0x128   f128        <-- rxtiming's own loop bound
   +0x12a   f12a          the AGC's sample counter
   +0x12c   energy.sum    the AGC's accumulator
   +0x130   rx_samples    the cursor doing the writing
```

So there are fourteen shorts.  The fifteenth pull overwrites `f128` while
`rxtiming` is looping on it, which lets the loop run longer, which pulls
more, which reaches `rx_samples` at the nineteenth and corrupts the cursor's
low half; the twentieth dereferences whatever that made.  A one-sample
overrun escalates to a wild pointer within six more.

This is the object's own layout, so the original has the same bound.  It
holds because `f1ae < f1b0` keeps pulls to at most one per output in normal
operation -- two only while the timing loop is slewing.  It is a constraint
on the caller, and `receiver` should be checked against it.

Found by driving the interpolator past it deliberately: the fixture used
`f128 = 24`, which segfaulted.  Worth stating that the crash was the good
outcome.  Had the burst been a few bytes shorter it would have scribbled
only `f128` and `f12a`, identically on both sides, and the differential test
would have passed while measuring nothing -- finding 116b's failure mode,
which is silent.  The bound is now asserted in the fixture rather than
assumed.

### 124. Auditing the other two register-liveness bug claims

D34 was retracted for inferring a register's contents and getting it wrong
(finding 122).  Its own text grouped it with D30 and D32 -- "the same family
... a compiler reusing a register the author assumed was dead" -- so those
two were re-checked the same way, from the disassembly rather than from the
entry.  **Both stand.**  What separates them from D34 is worth recording,
because it is the thing to check on the next such claim.

**D32 stands.**  The claim is that `%ebp` still holds `sine[phase]` when
`V34ModulatorProcess` pushes it into the symbol delay line.  Dumping the
whole function and listing every write to `%ebp` -- not a mnemonic-filtered
grep, per finding 115c -- gives thirteen sites.  The last before the shift
loop at `0x735ae` is `movswl (%edi,%ebx,2),%ebp` at `0x734dc`.  There is a
reload, `mov 0x54(%esp),%ebp` at `0x73580`, but it sits on the exit path:
`0x73575`'s `jg 735ae` jumps over it.  So on the path that reaches the shift,
the register genuinely is not reloaded.

**D30 stands, and is a different kind of claim.**  It says
`V34InitializeImplementationSpecific` reads `dline` into `cursor` before
writing `dline`.  That is an argument about the order of three instructions
at `0x71d85`, `0x71da3` and `0x71daf`, all quoted in the entry -- no
inference about what a register survived.

**The discriminator.**  D30 and D32 reason within a straight-line window and
name every instruction in it.  D34 reasoned *across a call*, and the
instruction that refuted it (`xor %eax,%eax`) sat outside the window,
BEFORE the first store rather than between two of them.  A liveness claim
that spans a call, or whose window is chosen by where the interesting stores
are rather than where the register is written, is the one to distrust.

**Their tests do compare the affected state**, which D34's did not:
`t_v34ec` compares the echo cursor as an offset from `dline`, and the
modulator's delay line at `+0x1450+0xcbc` is not among the skips in the
`txmit` fixture.  Two fixtures were excluding all 0x20 bytes of
`struct v34_echo` when only the first 0x18 are pointers; narrowed, which is
16 more checks in `txinit` and 320 in `rxtiming`.

### 125. `f19c`, the RMS index, is initialised two levels up

Neither `rxinit` nor `rxtiminginit` writes it, and both `V34demodulate` and
`V34agc` then index `rms_buf` with it unchecked -- so it looks like the same
uninitialised-value family as D30 and D32.  It is not.  Six functions write
`0x19c`: the four increment sites in `V34demodulate` and `V34agc`, and
`dpskinit` (`0x5f542`) and `v34modeminit` (`0x5f892`), which both do
`xor %ecx,%ecx` and store zero.

So it is initialised, just further up the call chain than the two functions
that look like they should do it.  No deviation.  Recorded because "the init
function next to it doesn't set it" is not evidence of a bug, and answering
it took one search rather than an argument.

The `rxtiming` fixture now fills with `HARNESS_MALLOC_FILL` rather than
zeroing, and sets `f19c` explicitly to stand in for those two callers -- a
zeroed object would have hidden the question that fill exists to ask.

### 126. The diagnostic paths were untestable, so one of them was wrong

`updateAlpha`'s debug call was reconstructed as

```c
    dsplibs_debug_printf("updateAlpha %d %d\n", tag, *alpha);
```

with `tag` declared `int`.  All three of those are wrong.  The object has

```c
    dsplibs_debug_printf("updateAlpha%s: updated %d => %d\n", tag, was, now);
```

-- `tag` is a `const char *` that completes the function name (`adaptecho`
passes `"NE"`, for the near canceller), and the two integers are the alpha
BEFORE and after, with the before-value read at function entry rather than
where the message is printed.

It survived because `dsplibs_debug_level` ships at zero and every gate in the
object is `> 1`, so no differential run had ever entered a debug path.  The
comparison was structurally incapable of seeing it.  It was found only by
reconstructing `adaptecho`, which inlines the same call and therefore made
the same format string visible from a second angle.

**The fix is a test, not just a corrected string.**  Both sides already have
their own printf -- symmap prefixes it, because a shared one would interleave
the two transcripts -- so `dsplib_debug_capture_on` now routes each side's
output into its own buffer and the two are compared like any other result.
Raising `dsplibs_debug_level` and `ref_dsplibs_debug_level` together keeps
the control flow identical on both sides, so this costs nothing but coverage.

Worth stating what this generalises to: an untested surface is not merely
unverified, it is a place where a reconstruction and its reference can differ
indefinitely while every green test stays green.  debug.h already argued the
call sites were worth carrying because the format strings are the author's
own words.  That argument was right and incomplete -- carrying them without
comparing them meant carrying a guess.

### 127. `updateAlpha` divides by zero on a small negative energy

`(1 << (shift + 21)) / ((energy + 0x8000) >> 16)` traps when the divisor is
zero, which needs `energy` in [-0x8000, -1]: the normalisation loop stops
immediately because bit 30 is already set in a negative value, so no shift
rescues it, and `energy + 0x8000` lands inside 16 bits.

The original does the same -- this is not a reconstruction error.  `energy`
comes from `V34EchoEstimateDelayLineEnergy`, a sum of squares, so reaching a
small negative means that sum has already overflowed.  Not filed as a
deviation yet because reachability is exactly what has not been measured;
that belongs with task #47.  The test sweep avoids the range deliberately and
says so, rather than silently not covering it.

### 128. `modem_serrint`'s filter path replaces the value, not just the output

The per-symbol tick builds its complex sample three ways, chosen by the
receiver's flags: store the residual raw (bit 15), run a 60-tap FIR (bit 11),
or run `V34HilbertFilter`.  The first and third leave the residual alone.
The second does not:

```
   5d392:  mov 0x18(%esp),%ebx
   5d396:  mov 0x1c(%esp),%ebp      ; the filter accumulator
   5d3a1:  mov %ebp,%esi            ; <-- overwrites the residual
   5d3a3:  sar $0x10,%esi
```

`%esi` held the residual from `0x5d03f` and is what everything downstream
reads: the second history ring at +0x2aa8, the leaky energy estimate at
+0xa240, and BOTH cancellers' error terms.  So in this mode the canceller
adapts against the filtered signal rather than the raw one, which is a real
difference in what the loop is minimising, not a detail of where a value is
stored.

Reconstructed first with the residual carried through all three paths.  Modes
0 and 2 passed; mode 1 failed at the first byte of the +0x2aa8 ring, on every
one of its four flag combinations and nowhere else.  A sweep that had only
driven the default path -- which is the natural thing to write, since the
Hilbert branch is the one the receiver normally takes -- would have passed.

It is also not truncated to 16 bits: the register is used at full width for
the products and only the queue store narrows it, so a loud enough sample
feeds a value outside a short into the error terms.  Reproduced.

The 60-tap filter's delay line is `echo1.coeff_frac` -- the same array
finding 100 found DPSK.c using as its FSK delay line, and `V34EchoFilter`
using as fractional coefficients.  One region, three readers, none of them
live at the same time.

### 129. The shell demapper's tables are indexed with nothing bounding them

`shellDemapper` indexes three tables -- `t1` at +0xa48, `t2` at +0xb48 and
`t3` at +0xc48 -- with running totals of the eight sub-indices, and clamps
none of them.  The two clamps it does apply are to the *addends*, not to the
index:

```
   if (d - c >= n)  scale = n - sub[3];
   if (c >= n)      base  = n - sub[1];
```

so `t1[c]`, `t2[d1]` and `t3[d1 + d2]` are read wherever the sums land.  Both
correlation loops walk down from `t[d]` as well, so an over-large `d` reads
below the table as it goes.

Found by driving it: the first sweep allowed sub-indices up to 40, which puts
`d1 + d2` past 300 against tables of 128, and it segfaulted.  That is the
good outcome again -- a slightly smaller overrun would have read adjacent
object fields on both sides and, where those happened to agree, passed.

Not filed as a deviation: the caller is `demapFrame`, which is not
reconstructed, so whether it can produce an out-of-range group is exactly
what has not been measured.  Task #49 should check it, and the fixture now
asserts the bound rather than assuming it.

**`grid` is the third.**  `decodeDepth` indexes it with
`(23*hi + lo + 0x408) >> 2` into 529 entries, again unclamped, where `hi` is
a per-state parameter minus a residue.  A parameter of 200 -- unremarkable
on its face -- gives an index near 1200.  That cost a debugging round: the
reconstruction was correct and the fixture was reading past the table on both
sides, which presents exactly like a decode error.  Three tables in one file
with no bounds check is a property of the module, not an accident of one
function.

Worth noting alongside finding 123, which is the same shape in `rxtiming`:
in both cases the object relies on a caller-side invariant it does not state,
and in both cases the reconstruction found it by exceeding it rather than by
reading.

### 130. Two cursors, not one pointer and an index

`shell_correlate` is `t[0]*t[d] + t[1]*t[d-1] + ...`.  Written first as

```c
    sum += t[0] * t[d];  t++;  d--;
```

which advances the base AND decrements the offset, so `t[d]` names the same
element every iteration and the sum becomes `t[i] * t[d]` for a fixed `d`.
It compiles, it runs, it terminates, and it is a different function.

The differential test caught it on the first comparison.  Recorded because
the failure mode is invisible to inspection -- the line reads exactly like
the comment above it -- and because it is the second time in this session
that a helper factored out of three identical inlined copies was itself the
thing that went wrong.

### 131. `decodeDepth` is one engine with four preambles, not four decoders

The function opens with a compare chain on a table byte shifted right by two:

```
   589f1:  shr  $0x2,%eax
   589f8:  cmp  $0x2,%eax   ; je 58f7f
   58a01:  jg   58f3e
   58a07:  dec  %eax        ; je 58faf
           fall through     ;    58a10
```

Four targets, which reads as four constellation sizes each with its own
decoder -- and at 2075 bytes that would be the largest thing left in the
cluster by some margin.  Partitioning it says otherwise:

```
   exclusive to one case      114 bytes
   shared by two or more     1661 bytes   <- the common engine
   reached by no case         249 bytes

         63  0x58f3e   k3plus
         51  0x58f7f   k2
          0  0x58faf   k1
          0  0x58a10   k0
```

Two of the four contribute NO exclusive bytes at all -- they jump straight
into the shared engine -- and the other two contribute 63 and 51 bytes of
setup before doing the same.  So there is one Viterbi core, entered four ways
with different parameters, and the reconstruction is one loop nest plus a
short switch, not four algorithms.

Worth recording as a measurement rather than an impression, because the
impression was wrong in the expensive direction: budgeting four decoders
would have justified deferring the whole cluster, when what is actually
there is comparable to `demapFrame` beside it.

`cfgsplit` needed `--entries` here, as it did for `rxtiming` (121k): the
dispatch is a compare chain, so there is no jump table for it to find, and
without the four targets named it reports the entire function as
unreachable-by-any-case.

### 132. `decodeDepth` is two identical halves, not one pass

Beyond finding 131's four-preambles-one-engine result, the engine itself
repeats.  `0x58a40..0x58c37` and `0x58c40..0x58e33` are the same ~500-byte
block twice over, differing only in which stack slots they use:

  - a six-tap dot product of the history at +0xa18 against two coefficient
    rows, taken from the same base twelve bytes apart;
  - each accumulator rounded toward zero (`test`/`lea 1(%r)`/`js`), shifted
    down 14, masked against a value derived from `fa46`, compared, adjusted
    and shifted down 7 again;
  - a six-entry history shift at +0xa18, moving each short up one place;
  - a `kLookup` index assembled from three separate pairs of bits.

Two halves of four fields each is V.34's 8D frame seen as two 4D halves,
which is also what `putFrame` emits (finding in its header: four groups of
four).  So the reconstruction is one function called twice, not 2 KB of
straight-line code -- the same shape the AGC and the dequeue prologue turned
out to have.

Recorded before writing it, because the two halves use different stack slots
throughout and that is exactly the kind of difference that reads as
significant and is not.  The measurement to make when writing it is whether
the two blocks are bit-identical in behaviour or differ somewhere subtle, as
`V34agc` and `V34demodulate` did over a single `+0x200`.

### 133. `receiver`'s shape, recorded before writing it

4326 bytes, 128 blocks, no jump tables.  The call sequence gives the spine,
and it is the per-symbol receive chain end to end:

```
   V34TimingFilter          per output, inside an interpolation loop
   TimingV34                once
   V34EqualizerFilter       once
   V34TimingPrefilter
   V34EqualizerUpdateDelayLine
   decoderv34
   V34EqualizerAdapt  /  V34EqualizerCenterAdapt  /  V34EqualizerClearCenterTaps
   V34scrambler       x2
   V34EqualizerCleanUp
   sysdep_memset
```

**The opening loop is rxtiming's, and NOT quite.**  Same interpolation --
`wa = f1ac`, `wb = f1b0 - f1ac`, the two Q14 cross-products, the packed
`(Q << 16) | I` -- and the same one-or-two-pull wrap structure.  But the
`f244`/`f246` copy sits BETWEEN the two `V34demodulate` calls here, where in
`rxtiming` it precedes a single pull:

```
   5bee5:  pos -= wrap  (second time)
   5bef0:  call V34demodulate
   5bef5:  f244 = f240 ; f246 = f242
   5bf13:  call V34demodulate
```

There is also a parity test the interpolator does not have -- `test $1,%esi`
at `0x5bf96` diverts odd-numbered outputs to `0x5c532` -- so the two are
related but not the same function, and reconstructing this by adapting
`rxtiming` would be a mistake.  Finding 121l cost seven readings; the useful
lesson is not "the shape is X" but "look at THIS function's branches, not the
neighbouring one's".

Entry points worth naming when `cfgsplit --entries` is next pointed at it:
`0x5bed8` (wrap), `0x5c526` (single pull), `0x5c532` (odd output),
`0x5bfe2` (loop exit into TimingV34), `0x5c100` (the flag-0x400 branch after
the equaliser).

**Read so far, 0x5be90..0x5c722 of 0x5cf76.**

  - `0x5c532` resolves the parity test: odd-numbered outputs run
    `V34TimingPrefilter` and feed its result -- split as `(v >> 16, (short)v)`
    -- into `V34EqualizerUpdateDelayLine`.  So the interpolator produces a
    timing metric on every output and advances the equaliser's delay line on
    every OTHER one.  That is the two-samples-per-symbol structure, and it is
    why the loop looks like `rxtiming`'s but cannot share its shape.
  - `0x5c377` computes `|target|^2 >> 14` and compares `f124` against 0x40 and
    0x68, setting flags 0x200 and 0x100 at those two thresholds -- an
    acquisition ramp keyed on symbol count, not on signal quality.
  - `0x5c3d4` forms the cross-product `f20c*f212 - f20e*f210`, shifted up 2,
    into `f1fc`.  That is a phase-error term: the imaginary part of
    (decision* x target).
  - `0x5c40e` is `agc_rms` AGAIN -- the 36-tap `0x38e` window, the
    normalise/`sqrt_table`/index chain, identical to the one in v34rx.c.  A
    FOURTH copy.  It compares the result against `obj[0x22c]` and, if not
    greater, writes 10 into `obj[0]` -- which is the datapump's own status
    word, so this is the receiver declaring loss of signal.

Still unread: 0x5c722..0x5cf76, roughly 500 disassembly lines.

**Now fully read and written** -- findings 139, 140 and 141.  Both traps
recorded above held.  The third, the `f128` bound, turned out to be tighter
than finding 123 said, and for a second and unrelated reason.

### 134. 242 debug call sites were dropped, and nothing could have noticed

`debug.h` states the policy: the diagnostic call sites are carried because
the gating comparison is real control flow, and because the format strings
are the original author's own words -- "Discarding a call site discards the
annotation."

Counting them says the policy was not followed.  The blob makes **1670**
calls to `dsplibs_debug_printf`, spread over 399 functions; this tree makes
22.  Restricted to functions that ARE reconstructed, 58 of them are missing
242 call sites between them:

(The first version of this entry said "399 calls".  That was the count of
functions containing a call, not of calls -- an error found by writing
`tools/debugaudit.py` to make the count repeatable, which is a fair argument
for making one-off measurements into tools.)

```
   29  CALLPROG_Progress      16  V8Create           7  cadence_progress
   28  DialerProgress         16  GetDialerConfig    7  CALLPROG_Dial
   17  cadence_create         11  rebuildJMSequence  5  v8_process
                              10  v8handshak         ... and 49 more
```

They cluster in the early phases -- call progress, the dialler, V.8, Bell
103, V.23 -- and thin out through V.34, which is where the policy started
being applied rather than where it was written down.

**Why no test caught it.**  `dsplibs_debug_level` ships at zero and every
gate is `> 1`, so a missing call site and a present one behave identically
under every test in the tree.  Finding 126 made the same point about a
*wrong* format string; a missing call is the same hole one step further
along, and worse, because a wrong string can at least be found by reading
whereas a missing call leaves nothing to read.

**What it costs.**  Three things, in increasing order of seriousness.  The
control flow differs by a branch, which is the least of it since the branch
is not taken.  The annotation is lost -- and this reconstruction has leaned
on those strings repeatedly: "Near"/"Far" fixed which echo canceller is
which, "V34HSHAK: Freeze EC" named `v34FreezeEcho`'s flag,
"polyValue(k2)+polyValue(k)" identified an inlined function.  And the debug
transcript comparison built for finding 126 cannot test what is not there,
so restoring the sites is a prerequisite for that sweep rather than a
cosmetic tidy.

Recorded as one measurement rather than 58 separate defects, because the
cause is one decision applied consistently and the fix is mechanical.

**The number moves, and `tools/debugaudit.py` is the current answer** -- 237
across 54 functions as of `receiver`.  It is also a slight over-count, for a
reason worth knowing: it attributes a call site to the C function containing
it, so a site that moved into a helper split out of the blob's function is
reported missing.  `V34agc`'s one site lives in `agc_gain_sample` and
`receiver`'s seventh in `rx_train_point`; both are present, and both are
counted as gone.

### 135. The blob was built on 22 September 2005, 15:48 — and the seconds name six TUs

One of the dropped call sites (finding 134) is `FPM_AGC_init`'s:

```c
    dsplibs_debug_printf("AGC %s %s\n", __DATE__, __TIME__);
```

`__DATE__` and `__TIME__` are baked into `.rodata` at compile time, so the
blob carries its own build stamp.  There are six copies of each, at six
distinct addresses -- string pooling is per translation unit, so six copies
means six TUs that each contained such a line:

```
   0x03798  15:48:07   V32FP_recreate
   0x039a2  15:48:09   CreateV23Modem
   0x03b02  15:48:09   V22FP_create
   0x03d47  15:48:11   B103FP_create
   0x04144  15:48:16   fax_class1_create
   0x04e70  15:48:18   FPM_AGC_init
```

All six dated `Sep 22 2005`.  An eleven-second window, so this is one build
of one tree, not an archive assembled over time.

**Why it is worth more than trivia.**  The seconds are an INDEPENDENT check
on translation-unit boundaries, which `tumap.py` otherwise infers from
symbol ordering and address ranges.  `CreateV23Modem` and `V22FP_create`
share a second but NOT an address -- 0x39a2 against 0x3b02 -- so they are
two TUs compiled back to back, not one TU with two entry points.  That is
the kind of question the inference cannot settle on its own, and here the
compiler answered it.

The five functions named are also, by construction, one per TU that
contained the macro -- so each is a confirmed member of a distinct TU, and
`V32FP_recreate` and `V22FP_create` name two datapumps this reconstruction
has not reached yet.

Recovered only because finding 134 went looking for what had been dropped.
A call site nobody carried was holding the build date.

### 136. What the cadence detector's dropped strings name

Extracted ahead of restoring the call sites, because the strings are the
annotation and are useful on their own -- finding 134's argument in
miniature.  Twenty-four sites across the two functions.

`cadence_progress` (7):

```
   NO ANSWER state recognized
   CYCLES_COUNTER= %d
   BUSY cadence recognized
   CADENCE %s: CONDITION B SATISFIED
    CADENCE SERIRES COMPARISON ========================>
   CADENCE %s: CONDITION C SATISFIED
   CADENCE %s: CONDITION -- SERIES --- SATISFIED
```

`cadence_create` (17):

```
   TYPE %s                        Filter index %d
   Filter SubIndex %d             BUFFER LENGTH %d samples.
   INTEGRATION_LENGTH %d[ms]      LEVEL %d
   Disable CONGESTION detector    BUFFER LENGTH is INVALID!
   ============> %d               Disable RINGBACK detector
   Ringback index====> %d
   MAX_ON_TIME %d Buffers     MIN_ON_TIME %d Buffers
   MAX_OFF_TIME %d Buffers    MIN_OFF_TIME %d Buffers
   OFF_TIME_THAT_RESETS_CYCLE %d
   Detection Thresholds: levle_fix=%d,--> LEVEL_THRESHOLD=%d
   Cadence: Busy Tone loose detection is %d
```

**What they settle.**  The detector has three named acceptance conditions --
B, C and a "series" comparison -- which is structure `cadence_progress`'s
control flow shows but does not label; the reconstruction currently
describes them positionally.  `CYCLES_COUNTER`, `OFF_TIME_THAT_RESETS_CYCLE`
and `LEVEL_THRESHOLD` are the author's names for three fields this tree
calls something else.  And `levle_fix` is a typo in the original's own
source, which is the kind of thing that identifies a variable beyond
argument.

Two of the three `%s` arguments are a type name the caller passes, so
`TYPE %s` and `CADENCE %s:` share it -- the detector knows whether it is
busy, congestion or ringback, and says so.

Restoring the call sites themselves is still owed; this is the half that can
be banked without placing each gate exactly, and it is the half that helps
whoever reads `cadence.c` next.

### 137. The object holds TWO shell contexts, receive and transmit, 0x1be0 apart

`getFrame` takes one pointer and indexes it at `0x25e0`, `0x25f4`, `0x2a28`,
`0x2a30`... none of which are in `struct v34_shell` as mapped.  Subtract
0x1be0 from every one and they land exactly on fields already named:

```
   0x25e0 -> 0xa00  fa00        0x2a28 -> 0xe48  the bit callback
   0x25e4 -> 0xa04  fa04        0x2a30 -> 0xe50  frame[0]
   0x25f4 -> 0xa14  fa14        0x2a34..0x2a52 -> frame[2..17]
```

Twenty-three offsets, no exceptions.  So the V.34 object carries two
instances of the same structure -- the receive one at +0, which `demapFrame`
and `shellDemapper` work on, and a transmit one at +0x1be0, which `getFrame`
does.  They do not overlap: the struct is 0x1450 and the second starts well
past it.

**The two unmapped ones name themselves.**  `0x2a60` and `0x2a64` -- i.e.
+0xe80 and +0xe84, inside what was `pad_e74` -- are a 32-bit bit buffer and
its bit position: the position is compared against 15 and used as the shift
count for the buffer, and `lsbMask[n]` masks off the field width.  So the
transmit context has a bit *source* where the receive context has none, and
`putFrame`'s callback at +0xe48 is matched by `getFrame`'s at the same
offset.

**And it confirms the wide field.**  `getFrame` writes `frame[0]` with a
32-BIT store, spanning `frame[0]` and `frame[1]` -- the same pair
`demapFrame` writes and `putFrame` splits when the width exceeds 16, and the
only reason `frame[1]` exists.  Three functions, reached from three
directions, agreeing.

This is the encoder/decoder pairing the call graph's `--pairs` mode was
built to find, and it arrived with the structure already proved rather than
assumed.

### 138. The dialler's state machine, in the author's names

`DialerProgress` switches on `progress_state` with eleven bare cases, and
this reconstruction's header said so: "DialerProgress will name it; for now
the only thing established is that bound."  It never did name it, because
the twenty-eight debug call sites that would have were dropped (finding
134).

Restoring the strings names five of them, and four are pinned exactly -- each
small case block prints its own name before returning:

```
   progress_state 5   DIALER_WAIT_FOR_DIALTONE_STATE   -> DIALER_WAIT_DIALTONE
   progress_state 6   DIALER_WAIT_FOR_SILENCE_STATE    -> DIALER_WAIT_ANSWER
   progress_state 7   DIALER_WAIT_FOR_BONGTONE_STATE   -> DIALER_WAIT_BONG
   progress_state 8   DIALER_CALLING_TONE_STATE        -> DIALER_CALLING_TONE
   progress_state 9   DIALER_END_PARTIALLY_STATE
```

`DIALER_INITIAL_STATE` and `DIALER_END_STATE` are named by the same strings
but print from no case block, so placing them needs the surrounding
disassembly rather than a lookup.

**One of these corrects the reconstruction.**  State 6 returns
`DIALER_WAIT_ANSWER`, which this header glosses as "'@'".  The author calls
the state WAIT_FOR_SILENCE, which is the better name and the accurate one:
'@' waits for a period of quiet, not for an answer.  A gloss that reads
plausibly, was never wrong enough to fail a test, and is not what the code
is doing.

The same strings also name two procedures the dialler calls --
`PULSE_IS_DIALER_READY_PROC` and `PULSE_DIAL_DIGIT_PROC` -- a variable,
`digitToPulseDial`, and the original source file, `Dialer.C`.  That last is
the first direct evidence of an original filename; every other name in
`docs/modules.md` is inferred from symbol grouping.

### 139. `receiver` found a bug in `V34TimingFilter`, which its own test could not

Reconstructing `receiver` cost one real defect, and it was not in
`receiver`.  Its opening loop feeds `V34TimingFilter` a point interpolated
between two demodulated symbols; at the third rate tried, on the sixteenth
symbol, `timing_out[3]` came out 1 count low, and from there the timing
loop's integrator diverged and everything downstream with it.

The state made the fault look impossible.  Comparing every byte of both
objects, the filter's *entire* IIR state matched -- all eighteen shorts --
and so did `in0`, `in1` and thirty-nine of the forty high-pass history
entries.  The only difference was `hist[0]`, ours 5568 against the blob's
-5168, which is the discriminator this call had just computed.  But the
discriminator is a function of the four `iir[2..5][0]` values alone, and
those matched.  Two states that agree cannot produce outputs that differ,
so one of the two premises had to be wrong.

It was the second.  The object computes

```
   72611:  mov  0x3c(%esp),%eax        <-- &iir[3][0]
   72619:  mov  0x2c(%esp),%ebp        <-- pi
   72631:  add  $0x2000,%ebp
   72637:  sar  $0xe,%ebp
   7263a:  mov  %bp,(%eax)             <-- iir[3][0] = (short) of it
   ...
   7265d:  imul %eax,%ebp              <-- and multiplies %ebp, not (%eax)
```

so the four `imul`s take the registers the stores came *out of*, which still
hold the full 32-bit `(x + 0x2000) >> 14`.  The state gets the low sixteen
bits; the discriminator does not.  Reading them back from `iir[][0]`, as the
reconstruction did, is correct until the loop rings hard enough to push one
past a short -- here `(ni + 0x2000) >> 14` was 33359, stored as -32177 --
and then it is wrong by 65536 times a coefficient.

**Why its own test passed 114,000 checks and never saw it.**  `t_v34ec`
drives the filter with a smooth ramp from a zeroed state.  That is a
perfectly good test of the arithmetic and a poor one of the range: the
resonator never rings up far enough for any of the four to leave a short.
`receiver` drives it with the interpolator's output at three symbol rates,
which does.

**The lesson, and it is not "test harder".**  A truncating store and a
non-truncating use of the same value are indistinguishable across the whole
range where the value fits.  Wherever the object stores a shifted
accumulator into a `short` and then uses it again, the question "which one
does it use" has to be answered from the disassembly, because no input can
answer it until the value overflows.  This tree has three more such places
-- `V34demodulate`'s gained sample, `agcadapt`'s level, `TimingV34`'s
`whole` -- and they were re-read after this.  All three use the stored
short, and all three are written that way.

### 140. `receiver` is complete, and its own strings name six of its fields

4326 bytes, the last function of `V34RX.c`, and the file is now fully
translated.  The spine went in with the three flag gates held clear, then
one gate at a time, which is the discipline finding 133 asked for; the two
traps it named -- the loop is not `rxtiming`'s, and `0x5c40e` is `agc_rms` a
fourth time -- both held.

**What the seven debug strings settled**, and none of it was inferable from
the arithmetic:

| field | the original's name for it |
|---|---|
| `f124`  | `rxsymcnt` -- the received-symbol count |
| `f1c0`  | `pllcnt` -- the timing loop's state |
| `f798`  | `rtncount` -- the retrain counter |
| `f21a`  | `equerr` -- the equaliser's error, over 1024 symbols |
| `f224`  | `preerr` -- the predictor's, over the same window |
| flag 0x40 | a retrain request |
| flag 0x20 | an RRN -- renegotiation -- request |

So the block at `0x5c058`, which reads as three squared distances and a
saturating counter, is the retrain detector: it counts up while the
equalised point stops moving between symbols, and 0x8c consecutive still
symbols is a request to retrain.  The `V34EQU` pair says which of the two
adaptive stages is failing to converge, which is why they are published
together and never separately.

**The predictor is one engine run twice.**  `0x5c100` and `0x5c5eb` are the
same three-tap complex predictor over ONE set of coefficients at `+0x288`
and ONE history at `+0x294`: the first runs it on the equaliser output
(precoding), the second on the decision error, and only the second adapts.
The reconstruction is one function called twice with different `(x, y)`
pointers -- the same shape `V34agc`/`V34demodulate` and `decodeDepth` turned
out to have, and the third time that guess has been right.

Its rounding is asymmetric and deliberate-looking-but-not: the imaginary
accumulator starts at `+0x2000` and the real one is formed as
`b.hist_i - (0x2000 + a.hist_q)`, so the real axis rounds the wrong way by
half an LSB.  Reproduced.

**Six of its seven diagnostic sites are carried, and the seventh is too.**
`tools/debugaudit.py` reports one missing because the shifted-TRN2 report
sits in `rx_train_point`, a helper the blob does not have -- see the note
added to finding 134.  All seven are driven by a debug-transcript sweep
rather than merely written, which is what finding 126 asked for.

**And `receiver` never returns a value.**  Three `ret` sites, no common
`%eax`, and both callers -- `v34handshak` and `datapumpv34` -- are
unreconstructed, so nothing constrains it.  `void`.

### 141. `timing_out` is seven entries, and the burst's "headroom" is four live fields

Two corrections to finding 123, from `receiver`.

**Seven, not twenty-one.**  `timing_out[]`'s length was taken from the
pointer at `+0x2a4` on the assumption that everything between belonged to
it.  It does not: `receiver`'s predictor keeps its coefficients at `+0x288`,
which is entry seven.  The array runs `+0x27a..+0x287` and no further.

What makes this worth stating rather than just fixing is that a completely
independent constraint lands on the same number.  Finding 123 measured
fourteen shorts of receive-burst headroom before the burst overwrites
`rxtiming`'s own loop bound.  Seven outputs at up to two pulls each is
fourteen samples.  So `f128 <= 7` is both what fits the array and what the
burst survives, and the two arguments are unrelated -- one is about the
object's layout past `+0x27a`, the other about its layout past `+0x10c`.
Both fixtures now assert it.

**And the headroom is not headroom.**  Finding 123 described `+0x10c`
onwards as "not spare buffer -- it is the receiver's own bookkeeping", and
then listed the bookkeeping starting at `+0x128`.  The four shorts before
that are not spare either:

```
   +0x120   f120        the vectpp cursor
   +0x122   flags       every gate in receiver
   +0x124   f124        rxsymcnt
   +0x126   best_index
```

so the eleventh pull lands on `f120` and the twelfth on `flags`.  This was
not a reading -- it happened.  The first `receiver` fixture set `f1ae` by
hand and left `f1be` at the harness fill; `TimingV34` recomputes `f1ae` from
`f1be` every symbol, so the hand-set step survived exactly one call, after
which every output wrapped twice, twelve pulls ran, and `flags` became
garbage.  Both sides agreed on the garbage and the test passed 99% of its
checks while measuring nothing -- finding 116b's failure mode, reached by a
different route.

The fix was to stop setting the interpolator by hand and drive the fixture
through `V34SetupDemodulator`, whose six rates all have `f1ae < f1b0` and so
bound the pulls at one per output.  The real bound on the caller is
therefore not `f128 <= 7` but `f1ae < f1b0` *and* `f128 <= 7`; the first is
what keeps the second sufficient.

### 142. The call-progress states pin themselves, and confirm the messages

`callprog.h` recorded ten state names taken from the object's debug strings
and said of them: "The numeric values are not yet pinned;
CALLPROG_Create/Progress will settle them.  Listed here in the order the
strings appear, which is the order the compiler emitted them and so most
likely the enum order."

`CALLPROG_Progress` settles it.  It logs every transition as

```
    STATE:  %s --> %s
```

and both arguments come from `0x5d40(,%reg,4)` -- a table of string pointers
indexed by the state variable itself.  The table index IS the enum value, so
the guess from emission order was right, and is now confirmed rather than
assumed:

```
   0 CALLPROG_NO_LEGAL_STATE      5 CALLPROG_ANSWER_STATE
   1 CALLPROG_WAIT_DIAL           6 CALLPROG_END
   2 CALLPROG_DIALING             7 CALLPROG_END_PARTIALLY_STATE
   3 CALLPROG_WAIT_RING           8 CALLPROG_WFS_STATE
   4 CALLPROG_WAIT_TO_ANSWER      9 CALLPROG_BONGTONE_STATE
```

**And it checks the message enum for free.**  The table continues past those
ten with sixteen more strings, which are our sixteen `CALLPROG_*` message
names in our exact order -- `NO_MESSAGE` through `V8BIS_MODEM_ANSWER`.  Two
adjacent arrays the compiler laid out together, and the second is an
independent confirmation of an enum reconstructed from behaviour in phase 3.

`CALLPROG_Status_string` does NOT use this table -- it searches a separate
array of {code, string} pairs at 0x5dc0 -- which is what makes the 0x5d40
indexing unambiguous rather than a coincidence of ordering.

Note `CALLPROG_DIALING` names both state 2 and message 3.  The header now
spells the state `CALLPROG_DIALING_STATE` to keep them apart, and says why.

Two other strings from the same function are worth keeping: "Found 2100" and
"Found 2250" -- the V.25 answer tone and the V.8 ANSam variant -- and
"CALLPROG: 5 sec. silence was detected", which independently confirms
finding 138's correction that '@' waits for silence rather than for an
answer.

### 143. The dialler config's field names, and a V.8 version string

`GetDialerConfig`'s sixteen dropped call sites print every field of
`struct dialer_cfg` by name.  This tree had inferred names from the
`modem_get_param` getter that fills each field, and the two agree
everywhere -- which is itself worth knowing, since it means that inference
method is sound and can be trusted elsewhere.  The value is in the three
places they differ:

  - `DTMF_Gain1` / `DTMF_Gain2` for what this tree calls `dtmf_low` /
    `dtmf_high`.  Confirmed by offset, +0x08 and +0x0a.  Ours reads as a
    frequency pair; they are amplitudes, which the comment said but the
    name contradicted.
  - `toneOrPulseFlag` for `pulse_dialing`.  The author's name says it
    SELECTS between two modes; ours implies a boolean "is pulse".
  - `commaPauseDurLimit` -- with two m's -- for `coma_pause_limit`.  The
    getter is `GetComaPauseDurationLimit`, with one.  So the author typed
    the parameter name wrong and the struct field right, and this
    reconstruction copied the typo because the getter was all it had.

`V8Create`'s sixteen name the V.8 configuration the same way, and one of
them is not a field at all:

```
   V8: Create called, V8 version 23/09/03 .
```

A version string, dated 23 September 2003 -- two years before the 22
September 2005 build stamp of finding 135.  So the object is a 2005 build of
a V.8 module last versioned in 2003, which is a fact about the source tree's
age that nothing else in the binary records.

Its modulation list also fixes an order: V90, V34, V34HD, V32, V22, V17,
V29, V27, V23, V21 -- worth having when the V.8 capability bitmaps are
revisited.

### 144. Name tables outlive the code that printed them

The dropped-call-site sweep (finding 134) kept turning up state names, so I
looked for the tables rather than the call sites.  `.rodata` and `.data`
hold six arrays of string pointers, and `nm` names five of them outright:

```
   v8ControlName    0x5380  .rodata  11 entries
   v8SequenceName   0x53ac  .rodata   4
   v8StatusName     0x53c0  .rodata  19   the whole V.8 state machine
   CadenceNames     0x6208  .rodata   5   BUSY DIAL CONG RING INVALID
   statenames       0x7e80  .rodata  35
   StateName        0x6c00  .data    87   v34handshak's states
```

**The V.34 handshake's eighty-seven states are all here**, in index order,
and are now `include/dsplib/v34hshak.h`.

> **Corrected by finding 176.**  The claim below that nothing indexes
> `StateName` is wrong: `v34handshakinit` and `v34handshak` index it 533
> times over, through a relocation against the `.data` section symbol that a
> search for the name cannot see.  The claim about the two V.8 tables
> survives.  That is the 61 KB function tasks
#39-#45 are about, and the difference between planning it against "state 47"
and against `TX_PHASE3_ANS` is not small.  Four are placeholders --
`NOSTATE0`, `NOSTATE2`, `NOSTATE3`, `NOSTATE36` -- so the enum has holes and
eighty-three real states.

**`v8StatusName` and `v8ControlName` are referenced by nothing in this
object.**  No `.text` instruction indexes either.  The printf that used them
was compiled out or lives in another translation unit -- but the tables
survived, because a name table is separately addressable data and the linker
had no reason to drop a global.

That is the general point, and it is more useful than any single table: **a
name table outlives the code that printed it.**  Finding 134 recovered names
from surviving call sites; this recovers them where even the call site is
gone.  For an enum, the table IS the enum, in order, with no gaps to guess.

`CadenceNames` independently confirms `CADENCE_TONE_BUSY/DIAL/CONG/RING` as
0..3, which this tree had from behaviour.  `statenames` at 0x7e80 has 35
entries and is not yet read.

### 145. `statenames` is V.32's, and its lettering is the Recommendation's

The last of finding 144's six tables.  Thirty-five entries at
.rodata+0x7e80, and `V32StateName` is its accessor:

```
   84519:  cmp  $0x22,%edx
   8451c:  ja   84525            ; out of range -> "INVALID!"
   8451e:  mov  0x7e80(,%edx,4),%eax
```

0x22 is 34, exactly the last index, so the table's length is confirmed by
the code rather than inferred from the symbol size -- and the function
bounds-checks, unlike the three tables of finding 129.

The names are `STATE_A` through `STATE_Z`, plus `STATE_B2`, `STATE_D2`,
`STATE_F2`, `STATE_X2`, and then `END`, `CLEARDOWN`, `DONE`, `ERROR`,
`DONT_CARE`.

**The lettering is not the author's.**  V.32 labels its call-setup states A
through Z in the Recommendation itself, and the doubled ones are the spec's
own sub-states.  So this table is a direct index into the standard, which is
worth more than invented names would be: reconstructing V.32 can proceed
against the published state diagram with the mapping already established.

Emitted as `include/dsplib/v32state.h`.  V.32 is not reconstructed -- one of
the six build stamps in finding 135 is its translation unit
(`V32FP_recreate`, 15:48:07), so it is present in the object and simply not
reached yet.  Written down now because the table cost nothing to read and
would otherwise be rediscovered later.

That closes the table sweep: six tables, five named by `nm`, and between them
the state machines of V.8, V.32 and V.34's handshake plus the cadence types
-- three of which belong to code this project has not written yet.

### 146. Two of the sixteen names in finding 143 were mapped backwards

Finding 143 paired `struct dialer_cfg`'s fields with the author's names by
matching each field's `modem_get_param` getter against a plausibly
corresponding string.  Fourteen were right.  Two were not:

```
   param 0x11  GetPulseDialMakeTime   ->  "pulse_OffHookTime"
   param 0x12  GetPulseDialBreakTime  ->  "pulse_OnHookTime"
```

read directly out of `GetDialerConfig`, where each print is gated
immediately after the fetch it describes, so the pairing is not a matter of
judgement.  I had them the other way round, on the reasoning that "break" is
when the loop current is interrupted and therefore off-hook.

That reasoning is simply wrong about pulse dialling.  Off-hook is the
handset lifted, which CLOSES the loop -- the make condition.  Break opens
it, which is momentarily the on-hook condition.  The author's names are
physically correct and mine inverted them.

**What is worth taking from this.**  Finding 143 concluded that inferring
names from getters "is sound and can be trusted elsewhere".  It is sound at
roughly 14/16, which is a different claim, and the two it missed were the
two where a domain fact -- not a naming convention -- decided the answer.
The header now ties every field to a parameter ID taken from the
disassembly instead.

The general form: an inference that agrees with the evidence fourteen times
looks confirmed, and the confirmation is worth exactly as much as the two
cases nobody checked.  Both errors here were in the pair where I had
supplied a reason, which is the part that should have made me check rather
than the part that made me confident.

### 147. `GetDialerConfig`'s sixteen call sites restored, and what it took

The first function in the #50 sweep whose call sites are actually back rather
than just read.  Placement was unambiguous once the structure was clear: each
gate sits immediately after the `modem_get_param` it reports, jumping
out-of-line to a printf and back, so every string is tied to one fetch.

Three details that a plausible reading would have got wrong:

  - `pulse_BetweenDigitsInterval` reports the value AFTER the multiply by
    ten, not the raw parameter.  The `lea`/`add` pair runs before the gate.
  - the two gains print LAST, after everything else, and read back from the
    struct rather than from the register that computed them -- so they are
    sign-extended shorts where every other value is an int.
  - `DTMF_Gain1` is the field at +0x08 and `Gain2` the one at +0x0a, which
    is `dtmf_low` then `dtmf_high`.  The names invite the opposite reading.

I wrote that last one backwards, which makes twice in one file after finding
146.  Both times the error was in a pair where the names suggested an
ordering and I took the suggestion instead of the offset.

**On what the test proves.**  The transcript comparison passes, and swapping
the gains deliberately does fail it -- so the check has teeth.  But it was
written after the fix, so it is not what found the error; re-reading the
offsets was.  Worth separating, because "the test passes" and "the test
would have caught this" are different claims and only the second is worth
anything to whoever reads this next.

Sixteen down.  221 call sites remain across 52 functions.

### 148. The pulse dialler says "hook on" before the line moves and "hook off" after

`IsPulseDialerReady` carries three of the seven call sites in `call.c`'s pulse
group (seven, not eight: the earlier tally counted a comment that named
`dsplibs_debug_printf` in prose -- `debugaudit.py` counts textually and so did
I).  With these three, pulse.c is complete.  Two of them are the interesting ones:

    0x32fc  gate -> 0x3400   "call: %d: hook on...\n"    remaining
    0x33d8  gate -> 0x33e5   "call: %d: hook off...\n"   remaining

and they sit on OPPOSITE sides of their `modem_set_param`.  "hook on" prints
between setting `pulse_off_hook` and interrupting the line; "hook off" prints
after the line has been restored.

That is not a reading of the schedule.  At 0x3308 the store to `pulse_off_hook`
is ahead of the branch into the printing block, and a store cannot be hoisted
over a call to `dsplibs_debug_printf` -- the compiler has no idea what an
external function reads.  So the store precedes the print in the source, and by
the same argument at 0x33b6 the `modem_set_param` precedes the print there.
The asymmetry is the author's, and a host tracing the line sees it: the "on"
message arrives before the event and the "off" message after it.

The third is the entry message,

    0x32c9  gate -> 0x3391   "call: IsPulseDialerReady !(%u) (count %d)\n"

with `pulse_remaining` first and `pulse_elapsed` second -- read straight from
the object, ahead of the `remaining == 0` early out, so polling an idle dialler
still prints one line per tick.

### 149. A transcript comparison cannot see where a call site sits

Finding 147 said the transcript test was what caught two hand-written call
sites being wrong.  It is worth being precise about what it does not catch.

Six deliberate mutations were made to the three sites above.  Five were caught
immediately: swapped arguments, a dropped call, a misspelt string, a wrong
variable.  The sixth -- moving "hook off" to before its `modem_set_param` --
passed everything.  Of course it did: `modem_set_param` prints nothing, so
reordering a print around it changes no transcript.

This matters well beyond one function.  Roughly 220 call sites remain, and a
large share of them sit next to a callback; every claim about their position
was, until now, untested.  The fix is three lines in the harness: each side's
`modem_get_bits`, `modem_put_bits` and `modem_set_param` drops a marker into
its own capture buffer,

    << set_param 15 = 1 >>

so position relative to a callback becomes ordinary transcript content.  With
that in place all six mutations are caught, including the sixth.  Nothing is
emitted unless a test turns capture on, so the cost falls only on tests that
asked for it.

The general lesson is the one that keeps recurring here: a passing test is
evidence about the test until you have watched it fail.  Both times something
was learned this week it came from breaking the code on purpose, not from
running it.

### 150. Three ways the transcript test was still weaker than it looked

Finding 149 closed the ordering gap.  Reviewing that change turned up three
more, all of the same shape -- a check whose wording promised more than it
delivered:

**The anti-vacuity check stopped meaning anything.**  It read

    dsplib_debug_capture_text(1)[0] != 0

which meant "the reference printed something" right up until the markers were
added, after which it meant "the reference printed something OR touched a
callback".  Any function that calls `modem_set_param` now satisfies it with no
call site firing at all.  The instrument had defeated its own control.  Fixed
at cause: `dsplib_debug_capture_lines(side)` counts lines through the two
printf entry points ONLY, never through `dbgcap_note`, and every anti-vacuity
check now asks for that.  `t_v34rx.c` had the same idiom guarding its `saw`
flag, and `t_dialercfg.c` in its non-empty assertion; both switched.

**The markers were not uniform.**  `set_param`, `get_bits` and `put_bits` were
marked; `get_param` and `modem_debug_log_data` were not.  `get_param` is the
one that matters most -- every function in this library opens with `call_of()`,
which is a `modem_get_param` -- so position relative to the single most common
callback was still invisible.  Marked now, both sides, all five.

**One level tests one threshold.**  The transcript ran at
`dsplibs_debug_level = 2` on both sides.  Every gate in the object here is
`cmpl $0x1` + `ja`, and we spell all of them with one `DSPLIB_DEBUG_ON()` macro
that cannot express a site which disagrees -- so a site the original gated at
`> 0` would produce a byte-identical transcript at level 2 and nobody would
know.  The test now sweeps levels 1, 2 and 3, and at level 1 asserts the
REFERENCE printed nothing.  That assertion is a real claim about the blob, and
it is the only place a wrong threshold is visible.

Nine mutations now run against pulse.c and all nine are caught, including
"gate at > 0 instead of > 1", which only fails at level 1, and the two
reordering ones, which only fail because of the markers.  The mutation runner
is `tools/mutate.py` with the spec in `test/mutations/pulse.json`; it restores
the source in a `finally`, because an earlier hand-rolled version aborted
mid-run and left a mutated tree behind.

Seven down, 221 call sites remain across 51 functions.  The plan is to batch by
test binary rather than by size -- callprog (67 sites), v8 (64), dialer (38) --
so each batch is one test extension and one mutation run rather than six.

### 151. Finding 142 was half right: the state table stops at ten

Finding 142 pinned the ten `CALLPROG_*` state names from the table at
`.rodata+0x5d40`, which `CALLPROG_Progress` indexes with the state itself when
it logs `"STATE:  %s --> %s\n"`.  That part holds -- eleven of its 29 call
sites are that message, and the index IS the enum value.

It then said the table runs on past those ten into the sixteen MESSAGE names,
in our order, and treated that as an independent confirmation of the message
enum.  It does not.  Exactly ten `R_386_32` relocations fall in the range; word
11 onward are unrelocated bytes that belong to something else and merely
resolve to plausible-looking string addresses when you dereference them as
pointers -- `': create...\n'`, `'ate...\n'`, `'en'`.  Reading a table by
dereferencing past its end is how you get an answer that looks like the answer
you wanted.  The relocation count is the length; nothing else is.

The message names are a separate table at `.rodata+0x5dc0` in a different
shape entirely: `{code, name}` pairs, searched rather than indexed, which
`callprog_status.c` already reproduces.  So the message enum has one source of
evidence, not two.

One genuine cross-check does survive, and it is a nice one: state 2 is spelled
`CALLPROG_DIALING` -- character for character the same string as MESSAGE 3.
Two different enums, two different tables, one name.  `callprog.h` calls the
state `CALLPROG_DIALING_STATE` to keep them apart; the collision is the
original author's.

CAVEAT, so the next reader does not overrate this.  The ten strings in
`callprog_state_names[]` are TRANSCRIBED from the dump, not compared against
it by anything that runs.  The object indexes the table inline, so there is no
`ref_` entry point to difference against and no test touches it yet.  The
moment the first `"STATE:  %s --> %s"` call site lands, the transcript
comparison checks all ten for free -- until then they rest on my typing.

### 152. All eleven STATE sites are one idiom, checked rather than assumed

Eight of the eleven load a CONSTANT as the second argument -- GCC folded
`names[NEXT]` for a literal next state -- and return to a `movl $N,0x38(%edi)`
whose N matches the table index every time.  The other three (sites 1, 6, 16)
index the table twice, `0x5d40(,%esi,4)` and `0x5d40(,%ebx,4)`, and return
into the main body rather than to a constant store, which is a good reason to
suspect a second shape: a table-driven transition writing `state` at +0x2c
directly instead of the pending pair.

They do not.  All three targets -- 0x79b36, 0x79bb8, 0x79c7e -- are
`mov %ebx,0x38(%edi)` followed by `movl $0x1,0x3c(%edi)`.  Same pair, same
order, `ebx` simply carrying a runtime `next` where the others carried a
constant.  So one helper covers all eleven:

    if (DSPLIB_DEBUG_ON())
            dsplibs_debug_printf("STATE:  %s --> %s\n",
                                 callprog_state_names[cp->state],
                                 callprog_state_names[next]);
    cp->pending_state = next;
    cp->pending = 1;

Worth writing down mainly for the shape of the check.  "Eleven sites share a
string, so they share an idiom" is exactly the inference that produced findings
146 and 147, and it was cheap to test: three addresses, one disassembly.  It
happened to be right this time.  That is not a reason to have skipped it -- a
guess that turns out right and a fact are still different things, and only one
of them can be cited later.

(GCC's folding is itself invisible to the transcript: a helper emits an
indexed load where the object had a direct one, and both print the same
pointer.  Not worth reproducing.)

### 153. The two unnamed call-progress messages are 2100 Hz and 2250 Hz

`callprog.h` carried this, for messages 16 and 17:

    Two the object does not name: they are what the automode dual-tone
    detector reports, for its verdicts 3 and 5.  Named after where they
    come from, since the original's names for them were not recovered.

`CALLPROG_Progress` names them after all, not in the status table but in its
own diagnostics.  Verdict 3 prints `'Found 2100\n'` (gate 0x7a349, block
0x7a4e8, returning to `mov $0x10,%ecx` -- 16) and verdict 5 prints
`'Found 2250\n'` (gate 0x7a2e9, block 0x7a4f9, returning to `mov $0x11,%ebx`
-- 17).  Index and store agree in both cases.

So message 16 is 2100 Hz -- the V.25 answer tone, which is also what a fax or
data answerer leads with -- and 17 is 2250 Hz.  The names stay
`CALLPROG_DUALTONE_A`/`_B` because those are what the rest of the tree calls
them, but the comment no longer has to say the meaning is unrecovered.

This is the pattern finding 144 recorded from the other direction: the object's
own diagnostic text outlives what it describes, and it routinely says things
the code and the tables do not.

### 154. 29 sites placed in CALLPROG_Progress, 3 of them actually verified

All 29 are in, from the disassembly, and `make phase` is green.  That is a much
weaker claim than it sounds, and the mutation run says exactly how much weaker.

Seventeen distinct format strings, one mutation each: **3 caught, 14 not**.
The three are the eleven STATE transitions (through `request_state`),
`"CALLPROG: Time out"` and `"CALLPROG: LINE CLEAR TIMEOUT"`.  Everything else
is placed but unchecked -- if the string, the arguments or the position is
wrong, nothing here would say so.

The reason is structural, not laziness.  A transcript captures everything the
reference prints inside `CALLPROG_Progress`, and that includes `DialerProgress`
(28 sites) and `cadence_progress` (7), neither restored.  Any scenario that
dials or runs a cadence detector diverges on those, not on the sites under
test -- 63 of 83 scenarios did.  What is left is seeding a state and letting a
timeout fire, which reaches the transition machinery and nothing else.  So the
14 unblock when the dialer and cadence batches land, and not before.

Two smaller notes from the same run.  State 2 is skipped because it dials.
State 7 is exempt from its own timeout, so that scenario correctly prints
nothing -- which made the per-run "the reference printed something" assertion
wrong, and it is now asserted once for the sweep instead.  A test that demands
output from a run that should be silent teaches itself to lie.

The honest headline for this batch is "29 placed, 3 verified", and it is worth
writing that way round.  Placed and verified are different claims, and the
count that matters when something breaks later is the second one.

### 155. Not every gate is `> 1`: cadence_progress has a second threshold

`debug.h` said "every gate in the object is this comparison", and
`DSPLIB_DEBUG_ON()` has been spelled `dsplibs_debug_level > 1` everywhere on
that basis.  It is not true.

`cadence_progress` has seven call sites and they use TWO thresholds:

    0x7cdc3  cmpl $0x1     'NO ANSWER state recognized'
    0x7ceaf  cmpl $0x1     'BUSY cadence recognized'
    0x7cfe1  cmpl $0x1     ' CADENCE SERIRES COMPARISON ====>'
    0x7d080  cmpl $0x1     'CYCLES_COUNTER= %d'
    0x7d054  cmpl $0x2     'CADENCE %s: CONDITION -- SERIES --- SATISFIED'
    0x7d24f  cmpl $0x2     'CADENCE %s: CONDITION B SATISFIED'
    0x7d39e  cmpl $0x2     'CADENCE %s: CONDITION C SATISFIED'

The three at `$0x2` need level 3, not 2.  They are the per-matched-cycle
messages, which at level 2 would bury everything else -- a deliberate second
tier, not an accident.  `cadence_create` has one too ('Disable CONGESTION
detector', 0x7dcd1).  `DSPLIB_DEBUG_VERBOSE()` now spells it.

This is finding 150's prediction arriving, and it is worth being precise about
what caught it.  The level sweep did NOT catch it -- the sweep only asserts the
reference is silent at level 1, which these sites satisfy.  What caught it was
reading the gates, because `debugaudit --sites` prints them all and flags
anything that is not `cmpl $0x1`.  The sweep would have caught a wrong
threshold in a site already restored; the tool caught one before it was written.
Both were needed, and the cheaper one found it.

The general point: "every X in the object is Y" is a claim about 1670 call
sites, and it was made after looking at some of them.  It survived this long
because the counter-example is invisible at the level everything is tested at.

### 156. Callprog batch: 46 of 67 placed

  CALLPROG_Progress   29  placed (3 verified -- finding 154)
  cadence_create      17  placed
  CALLPROG_Dial        7  placed
  CALLPROG_Delete      4  placed
  CALLPROG_Create      3  placed
  cadence_progress     7  3 placed, 4 left

63 of 67.  (An earlier draft of this said "4 placed, 3 left" for
cadence_progress; it was 3 and 4.  `debugaudit --missing` says so and I did
not.)

The four left in `cadence_progress` are the three `CADENCE %s: CONDITION ...`
messages and `' CADENCE SERIRES COMPARISON ====>'` (the author's spelling),
and they are left ON PURPOSE.  They sit inside `match_unrolled`'s
branch structure, where "CONDITION B" and "CONDITION C" have to be attached to
the one-period and two-period tests in the right order -- and the two blocks do
not read the way the names suggest: 0x7d393 tests a flag that is already set
rather than computing its own verdict.  Getting B and C the wrong way round is
precisely findings 146 and 143, twice already, and both times the names
suggested an ordering that the offsets contradicted.  Better left with a note
than guessed.

Placement notes worth keeping:

  - `CALLPROG_Delete is exited` and `CALLPROG_Dial was exited.` are TAIL calls
    in the object (`jmp` at 0x794cd and 0x7a868, not `call`).
  - `CALLPROG Create <<` comes FIRST and `CallProgFP_Create >>` second, from
    gate order 0x79588 < 0x795e6.  The arrows say the opposite; the gates win.
  - `cadence_delete with CADENCE_DIAL_OBJ` is announced before `CADENCE_OBJ`,
    from the return targets 0x79493 < 0x7949a -- matching the deletion order,
    but read rather than assumed.
  - `APPLY_FILTER = %d` prints the result of the `MustNoiseFilterBeApplied`
    read, so it is after it; the gate sits later in the body than the statement
    it was placed at, so the exact line is not pinned.  The order against the
    get_param is, and the harness marks that now.
  - `BUSY cadence recognized` is a fixed string, not `c->name`, even though the
    three CONDITION messages next to it all use the name.  The author's.

### 157. "Ringback index" appears twice because it is in the congestion branch

`cadence_create` prints the filter index once per tone, and the string is not
the same each time:

    busy         '============> %d\n'          before GetMaxBusyCadenceOnTime
    congestion   'Ringback index====> %d\n'    before GetMaxCongestionCadenceOnTime
    ringback     'Ringback index====> %d\n'    before GetMaxRingbackCadenceOnTime

So `'Ringback index====> %d\n'` is in .rodata twice, once for each of two call
sites, and one of them is in the CONGESTION branch, mislabelled.  The busy one
is the same line again with the label rubbed out and the arrows left behind.
A copy-paste, three ways, and the author's.

Which of the two identical strings belongs where is not guessable from the
text -- they are byte-identical.  It is read from the return targets: the
blocks at 0x7dfb8 and 0x7dfcd jump back to 0x7dc12 and 0x7de94, which load
parameter 53 (`GetMaxCongestionCadenceOnTime`) and 47
(`GetMaxRingbackCadenceOnTime`).  That also pins the position -- after the
filter-index read, before the windows -- which nothing in the string says.

Nobody would notice this from the output; two identical lines print either
way.  It matters because the next person to read
"Ringback index====> 3" in a log while debugging congestion detection would
lose an hour to it, and now the comment says so.

### 158. DialerProgress's state announcements, tied to their cases

`dialer.h` records that two of the nine `DIALER_*_STATE` strings could not be
tied to a case: "Named by the same strings but not yet tied to a case:
DIALER_INITIAL_STATE and DIALER_END_STATE.  Cases 0 and 10 print no state
name."  That is now resolved, and the reason they looked unaccounted for is
better than a missing print.

Each case announces itself, and `--sites` gives the return target of every
announcement -- which is the case body it belongs to:

    'DIALER_INITIAL_STATE\n'            -> 0x7b191  (+0x0b1)
    'DIALER_END_PARTIALLY_STATE\n'      -> 0x7b122  (+0x042)
    'DIALER_END_STATE\n'                -> 0x7b122  (+0x042)
    'DIALER_WAIT_FOR_SILENCE_STATE\n'   -> 0x7b469  (+0x389)
    'DIALER_WAIT_FOR_BONGTONE_STATE\n'  -> 0x7b488  (+0x3a8)
    'DIALER_WAIT_FOR_DIALTONE_STATE\n'  -> 0x7b44a  (+0x36a)
    'DIALER_CALLING_TONE_STATE\n'       -> 0x7b4a7  (+0x3c7)

`DIALER_END_PARTIALLY_STATE` and `DIALER_END_STATE` return to the SAME address.
They are two cases sharing one body -- both do nothing but return their code --
so the compiler folded the bodies and left two announcements in front of one
tail.  Nothing about the strings says that; the equal return targets do.  This
is why the earlier pass concluded the two states "print no state name": it was
looking for two distinct bodies and there is only one.

`DIALER_INITIAL_STATE` returns to 0x7b191, well inside the function and not to
the shared tail, so it is a case with a body of its own.

Worth keeping as a technique: when two call sites carry different strings and
the same return target, the cases are folded, and that is a fact about the
control flow that no amount of reading the strings will give.

### 159. DialerAbort's three guards are not three of a kind

Our `DialerAbort` was three early returns and an action:

    if (d->progress_state > 10)  return;
    if (d->pulse_released != 0)  return;
    if (d->pulse_active == 0)    return;
    LastPulseDigitDialed(d->modem);
    d->pulse_released = 1;

Behaviourally right, and it reads as three guards of a kind.  The object says
otherwise.  Only the first returns:

    0x7bdef  ja  0x7be16   progress_state > 10 -> "Dialer was aborted - error."
                                                 -> RETURN
    0x7bdf9  je  0x7be24   pulse_released == 0 ? test pulse_active
    0x7be2c  je  0x7bdfb   pulse_active  == 0 -> the message
    0x7be50  jmp 0x7bdfb   released -> the message

Everything except the error path converges on 0x7bdfb, the gate in front of
`"Dialer was aborted.\n"` -- so an abort with nothing to release still says it
aborted.  Two guards skip the work; one guard is an error and skips the
message too.  The shape only shows up once the diagnostics are back, because
with them stripped all three branches genuinely are equivalent.

The function is now one condition rather than two returns, which is the only
way to place the message where the object has it.  No behaviour changed.

Two smaller notes: `"Dialer was aborted.\n"` is a TAIL call (`jmp` at 0x7be11,
not `call`), the third such in this sweep after CALLPROG_Delete and
CALLPROG_Dial; and the `" **** Dialer.C: LastPulseDigitDialed was called"`
message is printed AFTER the call it describes and BEFORE the flag is set.

This is the fourth time in this sweep that restoring diagnostics has said
something about structure that the stripped code could not (findings 136, 148,
158).  The pattern is consistent enough to plan around: a dropped call site is
not only a lost message, it is a lost constraint on the shape of the function
that printed it.

### 160. Where the return-target technique stops working

Findings 158 and 159 both turned on the same move: when several call sites
carry confusable strings, the address the block jumps BACK to says which
branch it belongs to.  It resolved DialerProgress's nine state announcements
and DialerAbort's three guards.

It does not resolve `GetNextDigitAndReturnNextState`, and it is worth knowing
that before spending an hour on it.  All six of its Tone/Pulse messages return
to the SAME address:

    0x7ae1b  gate -> 'Switching to Pulse'                        -> 0x7abd0
    0x7ae66  gate -> 'Switching to Tone'                         -> 0x7abd0
    0x7ae90  gate -> 'TONE_OR_PULSE_FLAG became TONE_DIALING'    -> 0x7abd0
    0x7aebb  gate -> 'TONE_OR_PULSE_FLAG became PULSE_DIALING'   -> 0x7abd0
    0x7aee9  gate -> 'Not permitted to switch to Tone'           -> 0x7abd0
    0x7af07  gate -> 'Not permitted to switch to Pulse'          -> 0x7abd0

0x7abd0 is the top of the parser loop.  Every one of these is a "note it and
carry on" branch, so they all rejoin at the same place and the return target
carries no information at all.

What is left is the GATE addresses, which are distinct and ordered.  Each sits
at its own point in the main body, so the discriminator is what PRECEDES each
gate -- the character test that reached it -- not what follows.  That is a
different and more expensive read, and it is the one this function needs.

Recorded as a negative result on purpose.  Three Tone/Pulse pairs is exactly
the shape that produced findings 143, 146 and 152, and the cheap technique
that worked twice in a row does not work here.  Knowing which tool fails is
worth as much as knowing which one works, and costs a lot less to write down
than to rediscover.

### 161. The tone-or-pulse flag's values, named by the author

Placing `GetNextDigitAndReturnNextState`'s call sites needed the read finding
160 prescribed: all six Tone/Pulse messages rejoin the loop top, so each was
tied to its branch by the character test that precedes its gate.  Reading the
whole function (per the DialerAbort method note -- for a 900-byte function the
whole read is cheaper than windows), the six resolve as:

    'T' at position 0        -> store 1 -> 'TONE_OR_PULSE_FLAG became TONE_DIALING'
    'P' at position 0        -> store 0 -> 'TONE_OR_PULSE_FLAG became PULSE_DIALING'
    'T' later, mixed allowed -> store 1 -> 'Switching to Tone'
    'P' later, mixed allowed -> store 0 -> 'Switching to Pulse'
    'T' later, forbidden     -> no store -> 'Not permitted to switch to Tone'
    'P' later, forbidden     -> no store -> 'Not permitted to switch to Pulse'

In each storing case the store precedes the call, so the announcement is of a
fact, not an intention.  The seventh site is a per-character trace, "Digit is
%c\n", printed after the NUL check and BEFORE the range check -- so characters
the parser only steps over are announced too, including ones past the jump
table's ceiling.

The strings also settle a naming question.  `dialercfg.c`'s config trace
already called the field `toneOrPulseFlag` (finding 146); these six give its
VALUES: 1 is TONE_DIALING, 0 is PULSE_DIALING.  Our field was named
`pulse_dialing` -- under which `pulse_dialing == 1` meant tone.  Renamed
`tone_or_pulse`, with `DIALER_TONE_DIALING`/`DIALER_PULSE_DIALING` for the
values.  The inference-from-getter-name trap again (finding 146): the field is
filled from `GetPulseDialingFlag`, and the getter's name describes the
parameter, not the stored sense.

### 162. DialerProgress's 28 call sites, and a correction to finding 158

All of Dialer.c's diagnostics are now placed: DialerProgress's 28, the
parser's 7, DialerAbort's 3, DialerCreate's 1, AnalyseDialString's 2 and
IsDialStringInvalid's 2.  Three things the placement settled:

FINDING 158 WAS WRONG about the two end states.  It read the equal return
targets of 'DIALER_END_PARTIALLY_STATE' and 'DIALER_END_STATE' as two case
bodies folded into one.  They return to 0x7b122 because that is the FUNCTION
EPILOGUE -- shared by every case -- and the bodies are separate: each has its
own pulse-release sequence, its own announcement, and its own return value
(END_PARTIALLY returns DIALER_COMMAND, END returns DIALER_DONE).  Our
reconstruction had them folded into a `case 9: case 10:` with a conditional
return; correct behaviourally, but only until the announcements had to go
somewhere.  Now unfolded.  The technique note in 158 stands, with a caveat it
should have carried: a return target identifies a case body only when it is
not also the epilogue.

THE STATE NUMBERING IS NOW COMPLETE.  The jump table at .rodata+0x614c pins
DIALER_INITIAL_STATE = 0 and DIALER_END_STATE = 10 -- the two finding 158
could not place, because their announcements sit inside their cases (0 prints
on every dispatch before asking the parser; 10 prints after its release
sequence).  States 1-4 (digit, gap, flash, pause) never announce and keep
descriptive numbers only.

28 OBJECT SITES ARE 24 SOURCE SITES.  The compiler triplicated the parser
dispatch (three GetNextDigitAndReturnNextState calls: state 0, states 2/4,
and end-of-digit), and with it the two messages inside the NEXT_DIGIT and
NEXT_PAUSE arms -- 'Samples left = %d' and 'Consequitive-Commas Dialing;
Number of samples is %d.' appear three times each with one source line
behind them.

Smaller facts worth their line:
- The pulse-count mapping runs on every poll BEFORE the digit starts and not
  after -- visible only because pattern 3 (New Zealand) announces 'Original
  digitToPulseDial %d' (no newline) inside the mapping.  Our pulse_count()
  call moved inside the not-yet-started branch to match.
- The gap state (2) and the pause state (4) both fill silence, but announce
  completion differently ('Done Generating silence between digits' vs 'Done
  Generating Dial-Pause') -- so they share a helper, not a case body.
  Another structural fact recovered from diagnostics alone.
- AnalyseDialString prints a dated banner on every analysis: "AnalyzeDialString:
  Updated 17 May 1999 00:50" -- Analyze with a z, while the symbol is spelled
  with an s.  The one timestamp anywhere in the blob's dialler.
- The grade table at .rodata+0x613c (FATAL/INVALID/TOLERABLE/VALID, finding
  56's names) exists FOR the debug output: 'Dial String Syntax is %s' in
  DialerCreate and IsDialStringInvalid, with "ILLEGAL!" for a grade past
  VALID, reachable by no caller.
- Every gate in Dialer.c is `> 1`; the level sweep asserts silence at level 1
  and byte-identical transcripts at 2 and 3.  Unlike cadence.c (finding 155),
  there is no second tier here.

Verification: 23/23 mutations caught by t_dialerprog's transcript comparison
(after adding an out-of-table-character case -- the per-character trace's
position relative to the range check is invisible without one), 2/2 by
t_dialer's.  The dialer batch is complete.

### 163. cadence_progress complete: the matcher forms have names

The last four of cadence_progress's seven call sites are its match verdicts,
and they name the three match forms:

    match_unrolled  -> 'CADENCE %s: CONDITION B SATISFIED'          (level 3)
    match_looped    -> 'CADENCE %s: CONDITION C SATISFIED'          (level 3)
    match_fixed     -> 'CADENCE %s: CONDITION -- SERIES --- SATISFIED' (level 3)
    match_fixed     -> ' CADENCE SERIRES COMPARISON ========================>'
                                                                    (level 2)

No CONDITION A message survives anywhere in the object -- either it was cut,
or A was the toneiir level test that precedes all three.  "SERIRES" is the
author's spelling.

Two placement facts the transcripts now enforce:
- The SERIRES banner prints on ENTRY to the fixed-pattern comparison, before
  the cycle-count check -- so a fixed detector that has not yet accumulated
  pattern_min_cycles announces a comparison it then declines to make.  Caught
  only by a setup whose min_cycles exceeds what the run accumulates.
- The three verdicts are the only `cmpl $0x2` gates outside cadence_create
  (finding 155's second tier), and the banner is not one of them: level 2
  shows the comparisons happening, level 3 says which ones succeeded.

With these, the callprog batch's cadence half is done: cadence_create 17/17,
cadence_progress 7/7, all mutation-tested (6/6) through t_cadence's new
level-1..3 transcript sweep.

### 164. V8Create's configuration trace names the V.8 fields

V8Create carries a seventeen-message configuration banner (sixteen call
sites -- the two "Call Functions" variants share one cross-jumped printf),
dated by its author: "V8: Create called, V8 version 23/09/03".  Every V8
message ends \r\n, unlike the rest of the blob's diagnostics.

What it names, none of which the reconstruction had:

- `v8->mode` is the SIDE -- "\tSide = %s" prints Caller for 0, Answer
  otherwise.  `fa48` is the "Operation Mode".  The two timeouts are the
  signal-detect and message-detect timeouts, in seconds.
- The LOCAL v8_cm's +0x0c is "ansPcmLevel" and +0x10 "ucodeForQts" -- the
  same offsets V8UpdateModemParameters uses for what the far end offered and
  the JM menu word, so the struct's field names cannot serve both instances
  and the header now says so.
- ext1[] is RAW CALL FUNCTION octets and ext2[] RAW PROTOCOL octets ("raw CF
  specified", "raw Protocol specified"), correcting the header's guess of
  country-code and vendor fields.  The presence bits are tested OUTSIDE the
  debug gates: the branch structure is hot, only the printing is cold.
- The modulation word, bit by bit: V90=(b0>>3)&1, V34=(b0>>5)&1,
  V34HD=(b0>>6)&1, V32=b0>>7, V22=b1&1, V17=(b1>>1)&1, V29=(b1>>2)&1,
  V27=(b1>>3)&1, V23=(b1>>4)&1, V21=(b1>>5)&1.  Call functions:
  Data=(b1>>6)&1, CallRxFax=b1>>7, CallTxFax=b2&1, V.80=(b2>>1)&1.
  b0 bit 1 is v8bisIndication, b2 bit 4 quickConnectEnabled, b2 bit 6
  lapmIndication.

initTxSequence's four sites came with it (they fire inside V8Create through
v8handshakinit, so the transcript comparison needed both):

- "V8: Initial %s message length is %d octets" -- JM when mode==1, and
  "octets" counting ten-bit characters.
- Three "V8: BUG -" messages: a raw call function or raw protocol declared
  present but empty (announced, THEN the present bit is cleared -- the
  repair follows the complaint), and no call function selected at all
  (announced, then data is selected as the default).

Sixteen banner messages re-test the level between each print rather than
sharing one guard -- so they are seventeen independent `if` statements in
source, not one guarded block; a single block would have compiled to one
test and a run of calls.

Verified by transcript comparison at levels 1-3 in t_v8util over six menu
variants (both call-function branches, both protocol branches, and each BUG
path); 12/12 mutations caught.  Field renames (mode -> side, fa48 ->
op_mode) deferred to travel with the rest of the v8 batch.

### 165. v8SequenceName: the object's one exported data symbol in V.8

V8SetMessage's entry banner indexes a GLOBAL table at .rodata+0x53ac:

    const char *const v8SequenceName[4] = {
        "V8_CM", "V8_JM", "V8_CJ", "V8_QC1A"
    };

Two things follow.  Selector 3 is the V.92 quick-connect QC1A sequence --
the reconstruction had guessed "CI" -- which fits the same story
ucodeForQts and quickConnectEnabled told in finding 164: the V.92
quick-connect support threads through all of V.8.  And the symbol is
exported, so the reconstruction now exports it too; it was invisible until
its one consumer's call site was restored.

V8SetMessage's four sites also mix line endings deliberately: the banner
ends \r\n like the rest of V8's trace, the three complaints (illegal type,
zero length, oversize) end in a bare \n.  The banner prints only AFTER the
selector is validated -- an illegal type gets its complaint instead, never
the banner -- and the oversize complaint reports the CALLER's length, not
the truncated one.  9/9 mutations caught via t_v8util's transcript sweep.

### 166. v8jm: the trace names the V.90 conditions, and two structural mistakes

The 22 sites across `rebuildJMSequence`, `evaluateRxJMSequence` and
`V8UpdateModemParameters` name the three anonymous flags the reconstruction
had been carrying as `flag_a`, `flag_b`, `flag_c`:

    V8Report: remote V90: mod - %d, digital connection - %d, pcmIndication - %d
    V8: on ANSWER: remote V90: mod - %d, ... , local - %d

so they are `v90_mod`, `digital_connection` and `pcm_indication`, and the
fourth value on the answering side is the local V.90 bit, `cm->b0` bit 3 --
the same bit V8Create prints as `V90` (finding 164).  `V8UpdateModemParameters`
closes with the same ten-bit modulation line V8Create opens with, so the two
sit either side of the negotiation: what was asked for, and what was settled
on.  `fdc4` is named by its own message, `V8Report: Finished with Quick
Connect`.

Cross-jumping merges the 0x107 and 0x109 announcements: both print
`call function DATA indication`.  The discriminator is where the merge lands
-- 0x109's test jumps to the `je` that precedes the gate, not to the store
after the print -- so V.80 genuinely shares the data announcement rather than
having none.  Both functions do this, so it is the author's choice, not one
compiler accident.  Twelve source sites in rebuildJMSequence compile to
eleven calls; the LAPM message shares the in-range message's call, and the
audit reads -2 as a result, exactly as V8Create reads -1.

TWO RECONSTRUCTION ERRORS FELL OUT, both in the second-extension scan, and
both invisible until the sites forced the region to be read properly:

  * The acceptance list was consulted before the filler, and a local
    extension that failed to match abandoned the word.  The object tests
    `ext_list[0]` BEFORE calling charFlip -- so the list-empty test cannot be
    in_list's inlined early return, it is a branch of its own -- and a failed
    local match falls through to the list rather than continuing.  Written the
    old way, a non-empty list that did not match still accepted the filler,
    and a configured extension that failed shut the list out.

  * With V.90 on offer the scan is a different loop entirely: it walks every
    marker word instead of stopping at the first, and it does not touch
    `fec2`.  Nothing else runs -- no acceptance list, no filler, and with no
    local field, nothing at all.

A third, smaller one: a first extension that matches in full sets `febc`
before breaking, which the reconstruction had left to the caller.

WHY NONE OF THIS WAS CAUGHT.  t_v8jm built what arrives with the encoder,
which cannot produce the inputs that discriminate: no encoded word carries
the second-extension marker except the filler, no encoded word gives
pcmIndication the value 1 that V.90 requires, an ext1 of 'G' is sent as
0x1c5 which is a capability word and never a function one, and the two
acceptance lists held characters (0x83, 0x54) that no encoded word can match
-- they are the pre-image under the wrong direction of charFlip.  So four
whole mechanisms were inert.  The hand-built table added in this commit
drives them: 0xc1 matches the word 0x107, 0x2a matches the filler 0x0a9, and
'*' and 'J' are the extension characters that land in the marker range.
Each divergence was confirmed RED against the old source before the fix.

The three V.90 conditions also had to be given different values from each
other, or the line reporting all three reads the same in any order: 0x141
withholds the modulation, 0x161 the digital connection, and 0x1c9 gives a
pcmIndication of 2 rather than 1.  29/29 mutations caught.

### 167. The V.8 handshake's ten messages, and v8StatusName

`v8handshak`'s diagnostics settle two things the reconstruction had only
guessed at.

THE TIMEOUTS ARE ANNOUNCED ONCE.  Three of them -- ANSam, CM, and the JM/CJ
message wait -- sit inside `if (fe64 == deadline)`, the conditional
increment that reads as pointless until the call site is back.  It is a
latch: the block where the counter reaches the deadline exactly announces
the timeout and steps past it, and every later block returns the same
failure silently.  Which message the third one names follows the side:

    "V8: Timeout waiting for %s message...", v->mode == 1 ? "CJ" : "JM"

and GCC left the ternary in both arms even though each arm knows the answer,
because the answering arm reaches the same print through a jump.

THE QCA1 BIT NUMBERS PIN THE WORD LAYOUT.  The three QCA1 lines report every
field twice -- from word 1 and from its repeat in word 4 -- and number the
bits over the whole received stream:

    bits24,26-28 and bits54,56-58   -> word 1 bits 5,3,2,1 and word 4's
    bit23 and bit53                 -> word 1 bit 6 and word 4's
    bits27-28 and bits57-58         -> word 1 bits 1-2 and word 4's

Every pair differs by thirty, which is three ten-bit characters, so word k
occupies bits 10k+10 to 10k+19 most significant first and bits 0 to 9 are the
character the hunt matched.  Since the acceptance test requires word 4 to
equal word 1, the pairs always agree: the lines exist to show the redundancy
survived.  One is labelled QCA1d and sits in the QCA1a arm, which the QCA1d
arm cannot reach -- the author's slip, reproduced.

And one message has no "V8: " and a bare \n: `V8 ANSAM Detected (CM ready)`,
which is what names the tone the turn-round branch has just accepted.

`v8StatusName` is the second exported table in V.8, .rodata+0x53c0, nineteen
entries indexed by what V8Process returns and ending in the author's own
`V8_LAST_ENUM`.  It names every status, and the names cross-check the
messages exactly: `f9d6` of 4 is the state whose timeout prints "Time Out
Waiting For CM" and whose status is V8_ANS_TIME_OUT_WAITING_FOR_CM, and the
same holds for CJ, ANSam and JM.  It also confirms `seq[1]` is the CJ
sequence (status V8_ORG_SEND_CJ is selected by `tx_seq == &seq[1]`) and that
ORG/ANS is the mode-0/mode-1 split V8Create prints as Caller/Answer.  The
five statuses the datapump treats as errors are exactly the five whose names
end TIME_OUT_WAITING_FOR_something.

### 168. v8_process was never differentially tested, and three things were wrong

Placing the wrapper's five sites meant reading it properly, and t_v8dp
tested only create, delete and the registration -- `v8_process` itself had no
differential test at all.  It had three defects:

  * A second V8_OK re-ran the whole negotiation.  The object breaks out when
    `f20` is non-zero: once a datapump change has been asked for, that field
    is counting down to it.

  * THE MODULATION BRANCHES WERE MISSING ENTIRELY.  Quick connect keeps the
    id the call asked for, but otherwise the next datapump is chosen from
    what survived the negotiation -- `b0` bit 3, bit 5, bit 7, the same three
    V8Create prints as V90, V34 and V32 (finding 164) -- as 90, 34 and 32,
    and nothing left is an error.  The reconstruction had only the quick
    connect arm, so a V.8 call that agreed on V.34 or V.32 would have
    negotiated correctly and then handed over to nothing.

  * The two fields published to the modem are written on ALL four accepted
    branches, not only the quick connect one.

`v8_create`'s message names parameter 8 -- read, never used, and printed as
`automode` -- and comes after the sample-rate check, so a call at a rate V.8
does not serve is refused silently.  The reconstruction's comment had it
backwards.

17/17 mutations on v8proc.c, 4/4 on v8dp.c, 5/5 on v8handshak.c and 11/11 on
v8hsrx.c.  Two mutations were dropped rather than left uncaught: a status
V8Process cannot return, and moving the delete message past a call that
prints nothing -- neither is observable, and a permanently uncaught mutation
is noise in the metric.

### 169. v8ControlName, the last of V.8, and the deferred renames

The third exported table, .rodata+0x5380, eleven entries indexed by
V8Control's request:

    V8CTRL_START_CM, V8CTRL_START_CJ, V8CTRL_START_JM,
    V8CTRL_CTRL3 ... V8CTRL_CTRL10

The last eight are placeholders in the original too, named after their own
indices, and no arm accepts them.  The three that matter name themselves by
the message they start, and each matches what its arm does -- START_JM puts
the answering side into hunt-for-CJ and its transmitter into sending, which
is exactly what follows a JM.  The request is announced whether it was
accepted or refused; only the unknown-request complaint reports a number
instead of a name, and only it ends \r\n.

V8agc's two messages name the AGC's saturation behaviour: `V8AGC, overflow =
0x%x,` (the author's trailing comma) once per clipped sample, and after ten
consecutive clipping blocks `V8: Due to overflow, looking for ANSam again...`
-- which is what the restart is FOR, and it fires only when the handshake was
waiting on the tone.

The phase detector's one message names the spacing it measures: `ANSAM phase
reversals detected delay = %d`.  It had never fired in any test: real ANSam's
reversals are far closer together than the window the detector accepts, which
is why t_v8sig's sweep counted detections and then discarded the count.  The
trace test places the run where four thousand steady samples would have left
it instead.

With these placed the V.8 module is complete -- 1670 blob sites, none left in
src/v8 that is not an inlining artefact -- so the renames finding 164
deferred have travelled with it: `mode` is now `side` and `fa48` `op_mode`,
in struct v8, struct v8_cfg and their sixteen files of users, with the
offset assertions moved to match.

13/13 mutations across v8sig.c and v8agc.c.  Two were dropped as
unobservable: moving the overflow message across a store it does not read,
and moving the delete message across a call that prints nothing.  One
mutation elsewhere is honest about what it does NOT show: "announce the LAPM
indication from the QCA1d arm as well" is caught because it ADDS a line, not
because the placement of the original is pinned -- nothing distinguishes a
line that never fires in one arm from one that is absent.
### 170. `fc8c` was unconditional, and only a whole-object comparison could see it

Task #38's `dpskinit` calls `V34SetupModulator(obj + 0x1450, 600, carrier,
0, 0, 1)`, and its differential test compares the whole 44 KB V.34 object
rather than a list of fields.  It disagreed at four bytes, +0x20dc, which is
the modulator's `fc8c`:

```
   dpskinit: object byte at +0x20dc   got 165, reference 15
```

`V34SetupModulator` was reconstructed with `m->fc8c = 0xf` in three arms of
its symbol-rate switch — 3000, 3200 and 3429 — because those are the three
the disassembly shows storing it in the obvious place.  The object has
**seven** such store sites, which is one assignment the compiler duplicated
along seven paths, and driving the blob directly settles it:

```
   baud   600 1234 2400 2800 3000 3200 3429 4800
   fc8c     f    f    f    f    f    f    f    f
```

for every carrier, every phase and both settings of `reset`.  So the store is
unconditional and belongs before the switch, and the reconstruction was
leaving five of the eight rates with the field untouched.

**The one site that writes 14 is inside D31's unreachable V.90 arm** of the
3200 case.  That is a second, independent confirmation of D31: a path that
cannot be reached is also the only path that disagrees about this constant.

> **Superseded by finding 216.**  D31 is retracted: the arm is live and the
> `v90` argument selects it, so 14 is a value `fc8c` really takes.  The table
> above is still exactly what the blob produces — for every carrier, every
> phase and both settings of `reset`, as it says — because the sweep that
> produced it held `v90` at zero.  The "second, independent confirmation" was
> the same blind spot twice, which is a sharper version of this entry's own
> lesson: a sweep confirms only what it varies.

**Why `t_v34ec` did not catch it.**  Its modulator test compares named fields
— `taps`, `f04`, `rows`, `row`, `sine_len`, `phase`, `wpos`, `wstep` — plus
the contents of every table a pointer selects.  `fc8c` was not one of the
names, and a field-by-field comparison can only check the fields somebody
thought of.  It now checks `fc8c` too, but the general lesson is the one
finding 116b and this entry share: **compare the whole object, and exclude
what has to be excluded, rather than listing what to include.**  The
exclusion list is visible and can be argued with; an inclusion list is
invisible and cannot.

The cost of the whole-object form is the pointer fields, which differ by
construction.  `t_v34hshak` handles that with an explicit skip list plus an
assertion that every entry on it was actually reached — so a stale entry,
which would be a silent hole, fails the test rather than widening it.

### 171. Two format strings were transposed, and only the transcript saw it

`v34info.c` was written from the disassembly with two pairs of strings the
wrong way round, and both survived every comparison of program state:

**`V34SetINFO0aBits`** has four "setting..." strings on two independent
tests.  The two that mention V.PCM were swapped:

```
   .rodata.str1.4+0xf4c  'setINFO0aBits - setting info0d (Digital) for V.PCM'
   .rodata.str1.4+0xf80  'setINFO0aBits - setting info0a for V.PCM'
```

and 0xf80 is the one on the session-variant-non-zero branch.  Getting it
backwards is invisible to state: the two branches print, and then do
different things for other reasons, so every byte of every object agreed.

**`V34GiveINFO1aBits`** computes a three-bit field and a seven-bit
bit-reversed one.  The three-bit field is what the object prints as
`upstream baud index` and the seven-bit one is what it prints as `Uinfo` --
which is the opposite of what the widths suggest, and the opposite of what
this reconstruction assumed.  Again invisible: both values were computed
correctly and stored in the right places, and only the labels were exchanged.

**What caught both** was the debug-transcript comparison
(`dsplib_debug_capture_*`), driven with `dsplibs_debug_level` raised on both
sides.  Finding 126 introduced that facility for a *wrong* format string and
finding 134 argued for restoring the call sites; this is the first time the
comparison has caught something in new code, and it caught two things in one
run.

**The general point is about which names to trust.**  The corrected reading
of `Uinfo` is less natural than the wrong one -- a seven-bit "baud index" and
a three-bit "Uinfo" is what anyone would guess -- and the naming in the
reconstruction now follows the object's own words against that intuition.
The same correction made the four `setINFO0aBits` strings fall on two clean
axes rather than one clean and one arbitrary, which is a second, independent
sign that the corrected assignment is the right one: `+0x6120` selects INFO0a
against INFO0d, exactly as `giveINFO0%cBits`'s letter already said.

**And one mutation survived the first fixture.**  `V34SetINFO0aBits` reads
the remote V.92 capability from a session byte on one branch and from the
object's own `local_v92` on the other; the sweep drove both from one
variable, so a reconstruction that read the wrong one passed.  Recorded
because it is the same shape as 116b and 123: the fixture, not the code.  A
test that sweeps two inputs together cannot tell them apart, and two inputs
that are read by the same-looking line in near-duplicate branches are exactly
the ones to separate.

### 172. `V34SetupModulator` was carrying an invented string, and it named two parameters

`tools/debugaudit.py --strings V34SetupModulator` lists three format strings
in the blob:

```
   V34SetupModulator: baudrate %ld, carrier %ld, preemp %ld, V90=%ld. fullReset=%1d
   V34SetupModulator: invalid baudrate %ld
   V34SetupModulator: invalid carrier %ld
```

The reconstruction had none of them.  What it had instead was

```c
    dsplibs_debug_printf("V34SetupModulator: carrier?\n");
```

**which appears nowhere in the object.**  Finding 126 is about a format
string that was wrong; this is one that was invented, and it is worse in the
same way finding 134 describes: nothing can see it, because the level ships
at zero and every other check in the tree is of program state.

**Restoring the entry diagnostic names two parameters.**  It prints five
values, and the fourth and fifth are the ones this reconstruction had as
`short phase` and `int arg4` — with `arg4` cast to void as unused.  The
object calls them `preemp` and `V90`.  So the fourth is the pre-emphasis
index, which the code below it already treats as one, and the fifth is a
V.90 flag that this function only prints.  Both are renamed.

A parameter that is read by nothing and printed by the one call site that
was dropped is exactly the parameter whose name is unrecoverable any other
way — and `V90` is a third pointer at the absent V.90 configuration D31 is
about.

> **Partly superseded by finding 216.**  `V90` is read: it selects the
> 3200-baud V.90 configuration, and D31 is retracted.  The naming argument
> stands — the diagnostic is still the only thing that gives the fourth and
> fifth parameters their names — but "read by nothing" was wrong, and the
> phrase that made it feel safe, "a third pointer at the absent V.90
> configuration", is what an unread parameter and a live one look like alike.

**And 600 baud is a real case, not the default.**  The object compares
against 0x258 and sends only the non-matching path to "invalid baudrate";
both then share one body.  A comment in the reconstruction said the opposite
— "Note it is the DEFAULT, not a match" — and restoring the diagnostic is
what showed it was.

**Both times the fixture had to be widened.**  The first sweep passed the
same variable as `preemp_index` and `v90`, so a printf with those two
arguments transposed passed; that is the third time in this session that two
inputs read by near-identical code were driven from one variable (see
finding 171).  It is the standing fixture defect of this project: **two
inputs that a wrong reconstruction could confuse must be swept
independently, or the test cannot tell them apart.**

### 173. `getbit` and `ApplyBulkDelay` stay deferred, and one byte past `getbit` is a jump

Task #38 left two of V34hshak.c's functions unwritten.  Both are file-local,
so `tools/symmap.py` — which renames `--extern-only` symbols — cannot give
them a `ref_` alias, and the project's answer for a local is to drive it
through a reconstructed caller (docs/coverage.md, eight of them).  Every
call site of both is inside `v34handshak`:

```
   5ec47:  call 5eaf0 <getbit>          ; getbit's own, recursive
   6484c:  call 5eaf0 <getbit>
   684bb:  call 5eaf0 <getbit>
   706a9:  call 5eaf0 <getbit>
   6639f:  call 5dd10 <ApplyBulkDelay>
   66af5:  call 5dd10 <ApplyBulkDelay>
```

so finding 117's conclusion holds for `getbit` as well: a #39–#45
dependency, not a #38 one.  `getbit` is also **recursive**, which the first
of those call sites is, and which is worth knowing before writing it.

**One byte past `getbit`'s declared end there is a jump into `setfinalrate`:**

```
   5eca1:  eb 0d    jmp  5ecb0 <setfinalrate>
```

`getbit` is 0x5eaf0 + 0x1b1 = 0x5eca1, so this instruction is *outside* the
symbol.  Either it is inter-function padding that happens to decode as a
two-byte relative jump landing exactly on the next function, or it is a tail
call the symbol size excludes.  Writing `getbit` will settle it; recorded now
because an unrecorded observation is not noticed twice.

### 174. What `v34handshak` is still waiting for, partitioned

Task #38 moved `v34handshak` from twenty-three unmet dependencies to
seventeen — `V34GiveINFO0dBits`, `V34GiveINFO1aBits`, `V34GiveProbeResults`,
`V34SetINFO0aBits`, `V34SetINFO0dBits` and `setfinalrate` all landed — and
`v34handshakinit` from four to three.  The seventeen are not one queue:

```
   python3 tools/callgraph.py --blocked | grep v34handshak
```

**Writable and testable now — five, 4,520 bytes:**

```
      30  VPcmV34ReportStartOfEchoAdapt
      30  VPcmV34ReportMiddleOfEchoAdapt
     350  VPcmV34SetMohMessageBits
     722  VPcmV34InterpretMohMessageBits
    3388  modulatevector
```

**Ready by call graph and NOT testable — two.**  `getbit` (433) and
`ApplyBulkDelay` (467) are file-local, so `objcopy` cannot alias them and
their only callers are inside `v34handshak` itself.  Finding 173, and
finding 117 before it.  `callgraph --ready` lists them because it models
callees, not testability; **`--ready` is not a work list on its own.**

**Blocked — ten.**  Two of them are one function away:

```
    6173  probeselect        needs chkForceBaudRate  (389, ready now)
    2629  v34handshakinit    needs VPcmV34SetMohMessageBits (ready now),
                                   preinitdigital (533, ready now),
                                   v34modeminit (1356)
```

so `chkForceBaudRate` alone unblocks 6.2 KB, and three ready functions plus
`v34modeminit` unblock another 2.6 KB.  The remaining eight —
`settxlevel`, `initdigital`, `V34SetINFO1aBits`, `V34GiveINFO1dBits`,
`VPcmV34GetMaxUpstreamRateIndex`, `indicateJaTransmission`,
`k56FlexPhase34`, `v90Phase34` — all bottom out in C++ that is not
reconstructed (`VPcmFloModem`, `K56FlexFloModem`, `V90ConstellationDesigner`)
or in `edprintf`.  **`v34handshak` cannot be finished before some of
`VPcmV34Main.cpp`'s C++ half exists**, whatever order the seven slices of
#39–#45 are taken in.

**Recorded because the obvious reading of the numbers is wrong.**  A summary
written from this session's own work called `v34handshakinit`'s three
remaining blockers "the shortest path", which they are not: it is one of
seventeen, five of the seventeen are writable immediately, and the single
largest reduction available is `chkForceBaudRate` — 389 bytes that release
6,173.  The partition, not the count, is the useful figure.

### 175. `edprintf` is the object's biggest single blocker, and it is a cipher

`tools/callgraph.py --blocked | grep -c edprintf` returns **145**.  Nothing
else left in the object is depended on by so much: it is what almost the
whole V.90/V.92 C++ half uses to say anything, and until it existed none of
those 145 could be started.  It is 351 bytes of string handling.

**It does not print its argument.**  It formats into a 256-byte scratch,
then emits each byte of the result as TWO characters — high nibble then low
— each with a rotating offset added, framed by `$!$ ` and `????`:

```
   edprintf("Hi")  ->  $!$ 8>8@????
```

The offsets come from `offsetarr`, ten ints at .data+0x94a0 holding
`4 6 2 7 1 9 3 5 8 7`, and `iEncodeOffset` steps through them once per
emitted character.  So the same byte encodes differently depending on where
in the stream it falls, and the log is unreadable without the table.

**This is a channel, not a log.**  The frame characters are what a decoder
looks for, and the manufacturer's own tool is the intended reader.  It
explains something that has been visible all along without being explained:
the object contains two entirely separate diagnostic mechanisms — the plain
`dsplibs_debug_printf` sites that findings 134 and 172 are about, which say
things like "Near Echo Canceller report", and this one, which says nothing
legible at all.  The V.34 and earlier code uses the first; the V.90/V.92
code uses the second.

**The counter is shared and survives across calls.**  `cEncodeChar`, the
other function in the file, encodes one whole byte through the same
`iEncodeOffset` — so the two interleave, and a `cEncodeChar` after N
`edprintf` calls returns a different character than it would after N+1.  A
successful `edprintf` zeroes the counter first and leaves it at
`2 * strlen` mod 10; the too-long path leaves it exactly where it was.

**All of that happens with diagnostics switched off.**  Only the final
`dsplibs_debug_printf` is behind the level test.  That is what makes the
function testable without reading a transcript, and `t_encode` uses it: it
reads the counter back through `cEncodeChar` at level zero.  Two probes are
needed, not one — `offsetarr` holds 7 at both index 3 and index 9, so a
single character does not name a position, while all ten consecutive pairs
do.  The test asserts that property rather than relying on it.

**The channel is now readable two ways.**  `tools/eddecode.py` decodes a
captured log -- the only option for one that came from the original binary --
and `dsplib_encode_plain` makes a build print plaintext instead (D40).  The
decoder's parse is exact rather than heuristic: '?' does occur inside
payloads, but `????` cannot.  A '?' at payload index i needs
`nibble == 15 - offsetarr[i % 10]`, an even i is a high nibble limited to
-8..7 by the arithmetic shift, and the only `offsetarr` entries of 8 or more
are at indices 5 and 8 -- of which one is even.  So a high-nibble '?' can
only fall at i congruent to 8 mod 10 and the longest possible run is THREE.
A payload may therefore end in up to three '?' that merge with the suffix,
which is why the terminator is the LAST `????` and not the first.

**`encode.c` is the object's last C translation unit.**  Its `STT_FILE`
entry is followed by `offsetarr`, `iEncodeOffset`, `temp.0` and
`cEncodedTemp.1`, its two functions sit at .text+0xb09b0 and +0xb09f0, and
the only `STT_FILE` after it is `pow.S`.  So this is a complete TU
reconstructed whole rather than a slice, which is rare this late in the
object.

### 176. `StateName` IS indexed — finding 144 was caught by the section-symbol trap

Finding 144 says of `StateName`, the 87-entry table at .data+0x6c00: "Nothing
in this object indexes it -- the debug call sites that printed it were
compiled out or live elsewhere."  **That is wrong.**

```
   600d3:  mov  0x6c00(,%eax,4),%esi     ; v34handshakinit
   600f8:  mov  0x6c00(,%ebp,4),%ecx
   60140:  mov  0x6c10,%ecx              ; StateName[4], V34HS_RECEIVE
```

`v34handshakinit` indexes it throughout and `v34handshak` does so **533**
times.  The table is not a survivor of deleted code; it is live, and it is
live in the largest function in the object.

**Why the search missed it, and why that is the third time.**  The reference
is `R_386_32` against the **section symbol** `.data` with 0x6c00 as an inline
addend, not against the symbol `StateName` — so a relocation search for the
name returns nothing, and `readelf -r | grep StateName` returns only
`V32StateName`, a function, matching as a substring.  `tools/dis.py`'s own
header lists three earlier instances of exactly this trap: the toneiir
pointers read as integers (finding 46), the `CP_*` tables read as
unreferenced (retracted at D10), and `cadence_create`'s bank-3 fallback.
This is the fourth, and the first to have been written into a finding.

**`tools/relocscan.py` exists for this and was not used.**  Its docstring
says so in as many words: "tabdump.py warns that a range contains
relocations; relocscan.py resolves them to names, which is what you actually
wanted."  The rule that follows is narrower and more useful than "use the
tool": **"nothing references this" is a claim about addends, not about
symbol names, and it cannot be made with `grep`.**

**What is confirmed.**  Finding 144's other negative claim survives the same
check: `v8ControlName` (.rodata+0x5380), `v8SequenceName` (+0x53ac) and
`v8StatusName` (+0x53c0) really are referenced by nothing, in either form.
`CadenceNames` and `statenames` are both indexed, which 144 and 145 already
had.  So the general point 144 makes — a name table outlives the code that
printed it — holds for the V.8 tables and not for this one.

**What it buys for #39-#45.**  The thirteen diagnostics in `v34handshakinit`
are state-transition traces, and they name THREE state machines, not one:

```
   V34HSHAKE: rxstate %s=>%s(tx %s, mst %s, [1]%ld, [2]%ld)
   V34HSHAKE: txstate %s=>%s(rx %s, mst %s, [1]%ld, [2]%ld)
   V34HSHAKE: microstate %s=>%s(tx %s, rx %s, [1]%ld, [2]%ld)
```

so the handshake is a receive machine, a transmit machine and a "microstate"
running together, each printing its own transition and the other two's
current state.  The five entries loaded by fixed address are the initial
states: `StateName[4]` `RECEIVE`, `[35]` `WAIT`, `[41]` `DET_SYNC`, `[43]`
`RX_DPSK`, `[54]` `SILENCEINFO`.

That materially changes how the split in fastpass.md should be made.  Eighty-
seven states over three concurrent machines is not seven slices of one
machine, and `cfgsplit` should be pointed at it before anyone assumes it is.

### 177. `--ready` models calls, not references — `preinitdigital` is not writable

`tools/callgraph.py --ready` lists `preinitdigital` (533 bytes), and it
cannot be written: it installs five things by ADDRESS that this tree does
not have.

```
   597bc:  mov  $0x0,%eax        <== R_386_32 scrambleGPC
   597cb:  movl $0x0,0x28(%edx)  <== R_386_32 Convolve16
   5993f:  mov  $0x0,%eax        <== R_386_32 scrambleGPA
   5994a:  mov  $0x0,%eax        <== R_386_32 descrambleGPC
   5995f:  mov  $0x0,%eax        <== R_386_32 descrambleGPA
```

None of the four scramblers is reconstructed — all four are themselves in
`--ready` — and `Convolve16` is a 128-byte `.rodata` table, not a function at
all.  A build would not link.

**The tool is right and the reading of it was wrong.**  `callgraph.py` builds
a CALL graph: a function is ready when everything it *calls* exists.  Taking
a function's address is not a call, and neither is naming a table.  So
`--ready` is a lower bound on what is blocked, and its answer for any
function that installs handlers or selects tables by pointer is unreliable in
one direction.

This is the second such caveat on the same output.  Finding 173 records the
first: `--ready` also lists file-local functions that no test can reach, so
it is not filtered by testability either.  Together:

> **`--ready` means "its callees exist".  It does not mean writable, and it
> does not mean testable.**  Check the relocations and the symbol binding
> before scheduling anything from it.

Both caveats bite hardest on exactly the code that is left, because
installing a handler by pointer is what an initialisation function does.

**What `preinitdigital` actually needs first:** `scrambleGPC` (146),
`scrambleGPA` (180), `descrambleGPC` (225), `descrambleGPA` (232) and the
`Convolve16` table — 783 bytes of code and 128 of data.  They are a coherent
unit and worth doing as one: the four are the V.34 scrambler and descrambler
for the two ends of the call.

**And `preinitdigital` names which end this is.**  It reads `f359c`, already
known from `setTimingStateParameters` as selecting one of two timing
parameter tables, and uses it to choose between the two polynomials:

```
   if (obj->f359c == 0x65) { scramble = GPC; descramble = GPA; }
   else                    { scramble = GPA; descramble = GPC; }
```

The two ends of a V.34 call must scramble with opposite polynomials, so
**`f359c == 0x65` is the originate/answer flag** — which also explains why
the timing ramp has two variants indexed by the same field.  One field,
two uses, and neither of them had a name until they were put side by side.

### 178. The shell's bit source and sink are the scrambler and descrambler

`struct v34_shell` has a pointer at +0xe48 that finding 137 read as a
callback: `getFrame` pulls bits through it in the transmit context and
`putFrame` pushes them into it in the receive one, so it was mapped as a
union of a source and a sink.  What fills it is `preinitdigital`:

```
   5993f:  mov  $scrambleGPA,%eax        ; -> obj+0x2a28, the TX context
   5994a:  mov  $descrambleGPC,%eax      ; -> obj+0x0e48, the RX context
```

so **the shell's bit source IS the scrambler and its sink IS the
descrambler.**  Two modules reconstructed months apart, joined by a third.

It also explains two shapes that looked arbitrary when `v34scram.c` was
written.  `scrambleGPC` returns `nbits - 16`, which is exactly what a source
returns -- the new bit position after supplying sixteen.  Both descramblers
return 0 unconditionally, which is what a sink's return is worth.  Neither
made sense as a scrambler's interface and both are obvious as a callback's.

**The widths do not match and the mismatch is not resolved.**  `v34scram.c`'s
functions take and return `short`; `v34_getbits_fn` as inferred from
`getFrame` uses `int`.  `getFrame` is not reconstructed, so which is the
original's declaration cannot be settled here; the union carries both
spellings rather than one being chosen.

**And `preinitdigital` confirms V34_SHELL_TX independently.**  Its two
identical blocks are at obj+0xa00 and obj+0x25e0, a difference of 0x1be0 --
which finding 137 derived from `getFrame`'s offsets alone.  Two functions
arriving at the same spacing from opposite directions is better evidence
than either had.

**One number it settles.**  Finding 129 had to infer t3's length from the
next known field and got 128 "which is the reassuring answer".  This clears
t1, t2 and t3 with the same `cmp $0x7f` bound, so 128 is measured now rather
than inferred -- and t3 is filled with -1 where the other two get zero, which
is what a cost table wants and zero is not.

### 179. `v34handshakinit`'s shape, before it is reconstructed

Everything it needs now exists, so this is the survey that should precede
writing it rather than an obstacle report.

**It takes a MODE and dispatches five ways.**  The second argument indexes a
jump table at .rodata+0x2d44, and two of the five entries are the same
address:

```
   5fb61:  cmp  $0x4,%ebp
   5fb6b:  ja   5fd10                  ; out of range -> the common tail
   5fb71:  jmp  *0x2d44(,%ebp,4)

   0  -> 0x5ff90     3  -> 0x5fd40     (the same body as 2)
   1  -> 0x5fed0     4  -> 0x5fb82
   2  -> 0x5fd40
```

So there are four bodies, not five, and an out-of-range mode is not an error
-- it runs the tail, which clears +0xaa3c and puts 0x10 in +0x2aa0.

**THE THREE STATE MACHINES ARE THREE WORDS**, at +0x3592, +0x3594 and
+0x3596, and every one of the thirteen diagnostics is the same idiom:

```c
    if (state != NEW) {
            if (DSPLIB_DEBUG_ON())
                    trace(...);
            state = NEW;
    }
```

-- compare, print the transition, assign.  That is why finding 176 found the
traces naming `rxstate`, `txstate` and `microstate` with each printing the
other two: they are three words set by the same three-line pattern, and each
trace shows the one changing plus the two that are not.

**What each mode starts the machines at**, in `v34hshak.h`'s names:

```
   mode      +0x3596            +0x3594          +0x3592
    0    SILENCEINFO (54)    RX_DPSK (43)          -
    1    SILENCERETRAIN(74)      ...               -
   2,3   SSEG (18)           RECEIVE (4)           -
    4    SILENCERETRAIN(74)  WAIT (35)      MOH_TONE (79)
```

so mode 4 is the Modem-on-Hold entry, modes 2 and 3 share the data path, and
mode 0 is the one that also calls `rxtiminginit`.  Which of the three words
is `rxstate` and which is `txstate` is NOT settled here -- it needs the
format string at each site read against its arguments, which is the first
job of writing it.

**What it calls:** `v34modeminit` (three of the four bodies),
`rxtiminginit`, `VPcmV34SetMohMessageBits`, `detectorinit` with `c1200_` or
`c2400_` on `f359c`, and `preinitdigital` transitively.  All of those now
exist, which is what makes it writable at all.

**What writing it needs that does not exist yet:** `StateName` itself, as 87
string pointers.  The traces index it -- finding 176 -- so the table has to
be emitted before a single diagnostic can be reproduced, and it is the
table `include/dsplib/v34hshak.h` already carries the names of.

**One thing to check first.**  The entry code reads +0x244 and +0x234 as a
pair, subtracts them and compares against 0x176ff, and on overflow resets
them to 0 and 0xfff15a00.  0x176ff is 95,999 and 0x69780 is 431,488, which
is added to +0x234 to make +0x238.  Those look like sample counts at some
rate -- 95,999 is 12 seconds at 8 kHz and 431,488 is not a round number of
anything obvious -- so the pair is a timer and the constants are worth
pinning before the states are.

### 180. The three machines are settled, and `StateName` had no other check

`v34handshakinit` is written, and the two things finding 179 left open are
closed.  Both were closed by reading the object rather than by reasoning
about it, which is the point of the entry.

**WHICH WORD IS WHICH.**  +0x3592 is `microstate`, +0x3594 is `rxstate`,
+0x3596 is `txstate`.  Finding 179 explicitly refused to guess, and the
method is finding 171's: at each of the thirteen sites one `%s` slot holds a
FIXED `StateName[k]`, and that k is always the value the site then assigns —
so the fixed slot names the word that is changing, and the two variable
slots are then named by the format string.

```
   0x60118  "rxstate %s=>%s"     new = StateName[4]   +0x3594 <- 4  at 0x5fd80
   0x60492  "txstate %s=>%s"     new = StateName[18]  +0x3596 <- 18 at 0x5fd5a
   0x6017b  "microstate %s=>%s"  new = StateName[41]  +0x3592 <- 41 at 0x5fda6
```

and all three agree in the other direction too: the rxstate trace's `tx %s`
reads +0x3596, the txstate trace's `rx %s` reads +0x3594, and both `mst %s`
read +0x3592.

**The second, independent sign** is the one finding 171 used to settle
`Uinfo`, and it is worth having because it does not depend on the
disassembly at all: +0x3596 only ever receives SSEG, SILENCEINFO and
SILENCERETRAIN, and +0x3594 only ever receives RECEIVE, WAIT and RX_DPSK.
Transposed, the *receive* machine would be the one entering SSEG, and the
table's own `TX_`/`RX_` prefixes say that is backwards.

**FINDING 179'S OFFSETS ARE WRONG BY FOUR.**  It quotes "+0x234 and +0x244",
which are the register-relative operands; the function does `lea 0x4(%ebx),
%edx` first, so the object offsets are **+0x238, +0x23c, +0x244 and +0x248**.
The same note applies to 155's mode table, which shows `-` for +0x3592 in
mode 0: 0x5ffec sets it to DET_SYNC, and mode 1 is the only body that leaves
the microstate alone.

`include/dsplib/v34fsk.h` already warns about this exact base, in as many
words: "`lea 0x4(obj); mov 0x248(that)` ... is an addressing artifact in all
three and is not evidence of a sub-object at +4."  The warning was written
down and finding 179 quoted the raw operands anyway, which is what makes the
rule worth stating rather than assuming: **an offset read off an instruction
is not an object offset until the base is checked.**

**The timer group, corroborated.**  A positive +0x248 is subtracted from
+0x238 and the difference kept if it is at most 95,999 **unsigned**;
otherwise the pair resets to 0 and -960,000.  `VPcmV34SetV90RateReneg`
(0xa1f2, and its `lea 0x4(%esi),%edi` at 0xa159) writes those same three
fields with those same two constants, which is what makes the group a group
and the reset arm a reset.  95,999 is one short of 12 s at 8 kHz; 431,488,
the stride added into +0x23c, is round at no rate this modem uses.  D43.

**The comparison is unsigned and that is testable.**  A base below the delta
makes the difference negative, which is far above the limit as an unsigned
and takes the reset arm.  A signed reconstruction differs only there, so the
fixture drives base and delta from separate variables — the two-inputs-one-
variable defect of 116b, 123 and 171 — and includes four such cases.

### `StateName` is a LOCAL symbol, so the transcript is its only check

Finding 176 established that the table is live.  What it did not say, and
what matters as much, is that **it cannot be compared directly**:

```
   304: 00006c00   348 OBJECT  LOCAL  DEFAULT  143 StateName
```

`objcopy --redefine-syms` renames a local symbol but cannot make it
linkable, so there is no `ref_StateName` for `t_v34hshak.c`'s fifteen-table
comparison to copy.  That is finding 173's trap — `--ready` models calls,
not bindings — arriving for a *data* symbol rather than a function.

So the only thing that reaches the eighty-seven strings is what the thirteen
traces print, and the sweep that prints them is not a supplementary check.
**It is the entire check.**  Two sweeps are needed and the second is the
interesting one:

- driving the three state words to the SAME value reaches all 87 names, but
  makes the three machines indistinguishable — every `%s` carries the same
  string, so feeding the `mst` slot from `rxstate` produces a byte-identical
  transcript.  That is exactly the defect this finding's first half was
  written to avoid, re-entering through the test.
- driving them a third of the table apart — i, i+29, i+58 mod 87 — separates
  the slots, and still reaches every name in every slot.

Both were mutation-checked, and so was the rest of the function: twenty-eight
mutations were applied and twenty-six were caught outright.  Four of them go
at the shared helper rather than at the states, because `hs_setstate` derives
its format string from the offset arithmetically and finding 130 is the
standing warning about what a shared helper can get wrong: permuting the
`fmt[]` entries WITHOUT touching the offsets is what actually pins the
string-to-word binding, and permuting each branch's two context arguments in
turn is what pins the slot order.  All four are caught; without the
distinct-triple sweep none of them could be.  The two
survivors are the useful part of the exercise.

- `(x & ~0x1d8) | 0x18` narrowed to `(x & ~0x1c8) | 0x18` survives because
  it is an EQUIVALENT MUTANT: the only bit the two masks differ in is inside
  the OR that follows.  Widening to a bit outside the OR is caught.
- the prologue's `txflags &= 0x7fff` narrowed to `0x3fff` survived because
  every body either overwrites +0x25c2 outright or masks bit 14 off again,
  so **only an out-of-range mode can observe the prologue at all** and the
  sweep had none with the high bits set.  A real gap, in the test rather
  than the code; mode 7 is in that sweep now and the mutation is caught.

### And it found four invented strings in already-committed code

`V34EchoCleanUp` was printing `"V34EchoCleanUp\n"`.  The object's string at
.rodata.str1.4+0xf848, referenced from that one site and nowhere else, is

```
   V34FLO: Echo running in Original Integer...\r\n
```

— the author using the clean-up to announce which of two echo-canceller
builds is live.  This is finding 172 exactly: a plausible string invented
from the function's own name, which could not be caught because nothing had
ever run that call tree with `dsplibs_debug_level` raised.  It surfaced only
because modes 0, 1 and 4 reach it through `v34modeminit` -> `txinit`, and
this is the first test to compare `v34modeminit`'s transcript at all.

**That is a class, not an incident, so the class was swept.**  Every string
literal this tree carries — 241 of them — was checked for presence anywhere
in the object's `.rodata` or `.data`.  A string that is nowhere in the blob
was written rather than read.  Three more turned up, all in
`V34EchoReportCoeff`, and the object's own are

```
   .rodata.str1.4+0xf878   ?======= Nothing to report =========
   .rodata.str1.1+0x2c84   ?%d %d %d %d %d %d
   .rodata.str1.4+0xf8a0   ?======= Coefficients[1..%ld]=========
```

— leading `?` on all three, a literal 0x3f.  **And the header takes an
argument the paraphrase did not**: `%edx` at 0x72298 is still `n`, the tap
count rounded down to a multiple of six.  So an invented string had also
hidden a missing argument, which is the part that would have mattered.

**EVERY literal, and the first attempt got that wrong.**  The obvious scan is
of `dsplibs_debug_printf("...")` call sites, and it finds 77.  But a format
reached through a VARIABLE has no literal at the call: `agc_gain_sample`
takes `fmt` as a parameter, and `hs_setstate` — written in this very session
— indexes a `fmt[]` table.  Both are invisible to a call-site scan, and the
three `V34HSHAKE:` strings this finding's first half is about were among the
invisible ones.  So the check scans every literal in `src/` instead;
comments are stripped and preprocessor lines skipped, which removes the only
legitimate non-blob literals in the tree (every one is an `#include` path).
That takes it from 77 to 241 and picks up `StateName`'s 87 entries as well,
which no call-site scan could ever have reached.

The general shape is worth keeping: **a checker written around the construct
you happen to have is blind to the construct you are about to write.**  The
first version was authored in the same session as the first format table in
the tree and could not see it.

Two limits, both real and both in the tool's own docstring. It is a
NECESSARY condition only — a string that is in `.rodata` but belongs to a
different function still passes — so it retires "invented from thin air" and
not "attached to the wrong site". And it says nothing about the ARGUMENTS,
which is the half that actually bit in `V34EchoReportCoeff`; only a
transcript comparison covers those.

It runs as part of `make test`, beside the licence firewall and for the same
reason: both are policies the differential tier is structurally blind to.
It self-tests in both directions — introduce a string in a `fmt[]` table or
in `StateName` and the build stops; the committed tree reports 241 checked,
0 invented.

**Why the strings were reachable but untested, and the stale comment that
kept them so.**  `t_v34ec.c` said of `V34EchoReportCoeff` that raising the
level "would still compare nothing, because neither logger records
anything".  That was true when it was written and stopped being true when
finding 126 built the capture facility — which the same file already uses,
forty lines earlier, for `V34SetupModulator`.  A comment that documents a
limitation someone else has since removed is worse than no comment: it tells
the next reader not to try.  Both functions now have their transcripts
compared, over seven tap counts and three coefficient patterns, and all five
mutations of the four strings and the one argument are caught.

That fixture needed its own storage: `V34EchoReportCoeff` dumps a hardcoded
144 coefficients whatever `taps` says (D28), which out of the shared 32-entry
array runs 48 shorts past the end of the struct into whatever the linker put
next — not the same on the two sides.  160-entry arrays keep the over-read
inside memory both sides own and seed identically.

**Where the debt sits.**  Finding 134 argued for restoring dropped call
sites; 126 built the capture facility; 147 was the first time it caught
something in new code.  This is the first time it has caught something in
code that had already passed a full differential test and been committed.
So the exposure is not "new modules might have wrong strings" — it is
**every module whose transcript has never been compared**, and `--invented`
narrows that but does not close it.

**One fixture defect, found by mutation and fixed.**  `run_setupreceiver`
dereferenced the detector's coefficient pointer at +0x3564 without seeding
it, so a reconstruction taking a path that never reaches `detectorinit`
CRASHED the test instead of failing it.  Seeded with the per-side dummies
like every other such pointer.  Recorded because a test that segfaults on a
wrong answer reports nothing, which is worse than a test that passes on one.

### What `v34handshakinit` unblocks

`callgraph.py --ready` now lists three functions whose only non-diagnostic
callee was this one, and all three pass the binding check finding 173 says
to make first (`readelf` gives `FUNC GLOBAL` for each, so `objcopy` can
alias them):

```
     258 B  T   VPcmV34InitiateHangUp
     259 B  T   VPcmV34InitiateRateRenegotiation
     274 B  T   VPcmV34SetV90RateReneg
```

`VPcmV34SetV90RateReneg` is the interesting one of the three: it writes the
same timer group with the same two constants, so reconstructing it is also
the second reading of D43's stride.  The other two are the callers that name
modes 2 and 1.
### 181. The shell mapper's two halves meet, and the transmit context aliases the trellis

`preinitdigital` and the four bit callbacks close the loop that finding 137
opened.  The V.34 object carries two instances of `struct v34_shell`, and
this is what each is *for*:

```
   receive  at +0        put_bits = descrambleGP*   sink
   transmit at +0x1be0   get_bits = scrambleGP*     source
```

`preinitdigital` installs the pair, and the choice is one comparison:
`f359c == 0x65` scrambles with GPC and descrambles with GPA, anything else
the other way round.  That is V.34's convention -- each end scrambles with
its own generator and descrambles with its partner's -- and it means one
flag decides the station's role for the whole datapump, since the same
`f359c` picks `setTimingStateParameters`' second table.

**The two contexts are not the same struct after all**, in one window of
twelve bytes.  Finding 137 checked twenty-three offsets against `getFrame`
and found no exceptions; the callbacks supply the exception:

```
   receive     +0xe74,78,7c  three scrambler words
               +0xe80        the bit POSITION, a short
   transmit    +0xe74,78,7c  three scrambler words
               +0xe80        a FOURTH word -- which is getFrame's bit buffer
               +0xe84        the bit position
```

The fourth word and the bit buffer are deliberately the same store:
`scrambleGP*` leaves its scrambled 32 bits there and `getFrame` reads them
out, so the register's last word IS the window.  The receive side folds its
three words down to sixteen bits and hands them off, so it has no fourth
word and puts its position in the space instead.

`preinitdigital` clears four ints and a position on one side and three ints
and a position on the other, and sets the transmit position to 32 -- past
the fifteen `getFrame` tests against, so the first call refills before it
reads.  That asymmetry read as an oversight until the callbacks were read.
It is two field sets.

**And the transmit context aliases the receive one's trellis.**  In shell
coordinates `modulatevector`'s eight-point output buffer is at +0xea0 and
its index at +0xec2, which land inside `sub[]` and `cost[]`; its trellis
would run to +0x2eac in object coordinates, where `hist_2aa8` already is.
Neither is a contradiction: only the receive context decodes, so `sub`,
`cost`, `trellis` and `state` exist only there -- which is exactly why
`preinitdigital` memsets those three on the receive side and on neither
other.  The object reuses the transmit context's unused tail for the
modulator's buffer and `modem_serrint`'s history rings.

### 182. `xyz` is derivable, and it stops where a signed int does

`initG248` copies a block of `xyz` into `t3` and computes nothing.  The
table is 945 ints carrying its own index header -- `xyz[0..19]` are offsets
into itself, and block n is `[xyz[n], xyz[n+1])`.

Block n is the cumulative count of the eight-fold convolution of a
rectangular window of length n: entry k is how many eight-tuples drawn from
0..n-1 sum to less than k.  So the length should be 8(n-1)+1 and the last
entry n^8 - 1, and both hold exactly for n up to 14.

From n=15 the blocks are SHORTER, and not arbitrarily: each ends on the last
entry that still fits a signed 32-bit int.  69 entries for n=15 where 113
would be needed, 58 for n=17, 56 for n=18 -- and in every case the next
entry of the true sequence is the first past `INT_MAX`.  Checked by
recomputing the convolution: every block is a prefix, and every cut is the
overflow point.

**The empty block is the caller's, not a hole.**  `xyz[16]` and `xyz[17]`
are both 831, so a ring of 16 gets no table at all.  `MMaxTable` runs
`... 14, 15, 17, 18` and `MMinTable` stops at 15, so 16 is the one size
`initV34` cannot ask for.  The table and its only caller agree about which
sizes exist, which is a stronger statement than either alone.

**This answers what finding 129 left open for `t3`.**  That entry recorded
three tables indexed with nothing bounding them, and said the caller-side
invariant had not been measured because `demapFrame` was not reconstructed.
For `t3`'s *initialiser* the invariant is now measured: the ring size can
only come from `MMaxTable` or `MMinTable`, both indexed 0..31 by a loop that
forces the range, so it is 1..18, and the longest block any of those selects
is 105 entries against a table of 128.  `initG248` cannot overrun `t3`.  It
says nothing about `shellDemapper`'s reads, which remain finding 129's.

Recorded rather than turned into a generator: `docs/fastpass.md` defers
coefficient derivation to task #47, and a byte-exact copy is what the
differential test proves.  The derivation is here so #47 does not have to
find it twice.

### 183. A reciprocal multiply that everybody reads as a divide by 100

`initV34` cost one debugging round, and the fault was a constant that is
almost a reflex:

```
   mul  $0x51eb851f
   shr  $0x3, %edx
```

`0x51eb851f` is the magic number for dividing by 100, and it is used here to
divide by **25**.  The multiplier is shared -- what picks the divisor is the
shift of the high half, and 100 needs `shr $5` (a total of 37) where this is
`shr $3` (a total of 35).

Written as `/100` the reconstruction agreed with the object on every bitrate
up to 24 and on nothing above, which is the worst shape a bug can have: the
smallest test case passes.  The differential sweep failed at `fa04` and
`fa06`, two fields apart, which localised it in one run.

Worth recording because the reading error is not in the disassembly, it is
in the pattern-matching: `0x51eb851f` is recognisable enough that the shift
stops being read.  Anywhere else this constant appears, the shift is the
thing to check.

### 184. `modulatevector`'s shape -- and it checks the bound `shellDemapper` does not

`modulatevector` is 3388 bytes, more than the other eight of this pass put
together.  This entry was written BEFORE it was reconstructed, the way
findings 99, 103, 111 and 133 recorded `V34TimingFilter`, `txinit` and
`receiver`, because planning against "the 3.4 KB one" and against the
structure below are different exercises.  It is now written and passing, and
the entry is kept as filed: every claim in it survived contact with the
differential test, which is the point of recording them in advance.

**It is the forward shell mapper, and it works on the TRANSMIT context.**
Every offset it uses lands on a `struct v34_shell` field once `V34_SHELL_TX`
is subtracted, with no exceptions in twelve checked:

```
   0x25f2 -> 0xa12 count    0x2628 -> 0xa48 t1     0x2a30 -> 0xe50 frame[0]
   0x25f4 -> 0xa14 fa14     0x2728 -> 0xb48 t2     0x2a34 -> 0xe54 frame[2]
   0x25f8 -> 0xa18 hist     0x2828 -> 0xc48 t3     0x2a38 -> 0xe58 frame[4]
   0x2604 -> 0xa24 coeff    0x2608 -> 0xa28 conv   0x2618 -> 0xa38 prev_k
```

So it is `shellDemapper` run backwards, against the same three tables, and
`getFrame` is its bit source exactly as `putFrame` is the demapper's sink.

**One call emits one point.**  The object carries eight complex points at
+0x2a80 and an index at +0x2aa2; a call with the index below 8 copies point
`n` to +0x25d0, bumps the index and tail-calls `txmit`.  Only when the index
reaches 8 does the mapping run, refilling all eight.  That is what
`scaleVector` scales: sixteen shorts is those eight points.

**Four top-level paths**, and a sweep that misses any of them proves little:

```
   index != 8, f25c2 bit 14 clear   copy the point, txmit
   index != 8, f25c2 bit 14 set     V34nlencoder the point, txmit
   index == 8, f25c2 bit  4 set     getFrame, then the mapping below
   index == 8, f25c2 bit  4 clear   the training counter, then the same
```

**The mapping**, in the order it runs:

1. A binary search over `t3` for the wide value `getFrame` left in
   `frame[0]` -- seven halvings, and the comparison is `ja`, UNSIGNED.
   That is why `preinitV34` fills `t3` with -1 rather than 0: `0xffffffff`
   is above any frame value, so the -1s are what stops the search walking
   into the part of `t3` that `initG248` did not fill.  The fill is
   load-bearing, not leftover.
2. Successive division and remainder against `t2` and then `t1` entries,
   which is the inverse of the demapper's convolution sums, producing eight
   sub-indices on the stack in pairs.
3. Four iterations, each consuming one group of four shorts from `frame`
   -- `frame[2+4i]` through `frame[5+4i]`, which is exactly the four groups
   `putFrame` emits -- and each producing two of the eight points.
4. Per point: a `quarter` lookup (416 shorts of packed byte pairs, the low
   byte taken SIGNED), a six-tap precoder over `hist` with `coeff`'s two
   rows of six, a shift-and-mask quantiser, the differential rotation
   through `prev_k` as a four-case switch on `(-x) & 3`, and a trellis step
   through `conv` and `smIndex` (16 shorts).

**It names `fa16` and `faa74`, which were both blanks.**  `fa16` is the
convolutional encoder's feedback mask -- `state = (state ^ conv[idx] ^
((state & 1) ? fa16 : 0)) >> 1` -- so 24 is `0b11000`, a two-tap generator
for the 16-state code, against one tap each for the other two.  That
retracts D47, which had filed the 24 as an unexplained break in a pattern
that turned out not to be a pattern.  A second reading, `fa16 == 64`,
selects a hand-unrolled six-register form of the same recurrence over
`fa2c[0..5]` -- which is also what `fa2c` is for.

`faa74` is a symbol counter with a purpose: it is compared against `fa00`
(the span J), and when it reaches it the object sets `data_enable` and
`f25c2` bit 4 -- which is the transition out of training and into carrying
data.  `preinitdigital` clears it, and this is the only thing that reads
it.

**And it bounds its sub-indices, which is what finding 129 wanted.**  Each
of the four is compared against `count` with `jae` and diverted to a fixup
when it reaches it.  Finding 129 said of the demapper's three unclamped
tables that "whether it can produce an out-of-range group is exactly what
has not been measured".  For the ENCODE side it is now measured: it cannot,
because it checks.  The decode side keeps the finding -- `demapFrame` is a
different caller -- but the intended invariant is now stated by the object
itself rather than assumed: every sub-index is below `count`.

**What a fixture will need -- SUPERSEDED**, and worth keeping only for what
it got wrong.  The prediction was that a fixture calling straight in would
take SIGFPE on the `idiv` against `t1` and `t2`, since `preinitV34` zeroes
both.  The real fixture runs `preinitdigital` then `initV34` and so never
tested it; the claim was never more than plausible and is recorded as
unverified rather than as fact.  `quarter` and `smIndex` are emitted, and
the `0x5a910` path this entry called unread is the training counter above.

The fixture that exists is in `t_v34shell.c`, and the thing it turned out to
need was not the divisions at all but TWO bit sources -- the real scramblers
and a synthetic pair -- for the reason finding 185 gives.


### 185. `modulatevector` found a bug in `getFrame`, whose own test could not

`getFrame` was committed passing 1.6 million comparisons.  The first thing
to drive it from a REAL caller disagreed on the second call.

The fault is in the split path, the one taken when the wide field is more
than sixteen bits:

```
   frame[0] = bitbuf >> pos
   pos = get_bits(obj, 0)          <-- the reconstruction
   (void)get_bits(obj, 0)          <-- the object
   pos = bitpos                        re-read from the object
   frame[1] = (bitbuf >> pos) & lsbMask[nb & 15]
```

The object throws the callback's return value away and re-reads `bitpos`
from the field.  That is not the same thing, because the scrambler
callbacks return `pos - 16` and never write the field: the position the
second half shifts by is the one the refill loop left, unchanged, while only
the buffer has moved on.  Taking the return shifts by a negative count
masked to sixteen and reads a different field entirely.

The refill loop is the other half of it -- the object stores `bitpos` back
on EVERY pass, not once at the end, which is what makes the re-read
meaningful.

**Why the original tests could not see it, and this is the interesting
part.**  Two conditions have to hold together: the wide field must exceed
sixteen bits, and the callback's return must differ from the position the
refill loop stored.  `getFrame` had TWO tests, and each covered one.

```
   differential test   nb runs -1..20   split path YES
                       bitsrc returns 0, which is also what the loop
                       stored, so the two readings agree      NO
   round-trip test     nb runs 1..16    split path NO
                       rt_source returns pos - 16, which differs YES
```

So neither fixture was careless: between them they covered both conditions,
and the bug lives only where the two overlap.  That is a more uncomfortable
result than a gap in one test, because both tests look thorough on their own
and the coverage argument has to be made across them rather than within
either.

The round-trip sweep now runs to 20, which puts both conditions in one
fixture and closes it permanently.

**What found it.**  `modulatevector` is the first caller that runs
`preinitdigital` and `initV34` and then drives `getFrame` through the
scrambler `preinitdigital` installed -- so all three conditions arrive
together for the first time.  Localised by swapping the synthetic bit source
back in: with an identical deterministic source on both sides the buffers
matched and only the POSITION diverged, which named the field.

This is the same shape as finding 139, where `receiver` found a bug in
`V34TimingFilter` that the filter's own test could not reach.  The lesson is
the same one and slightly sharper here: a leaf test proves the leaf against
the inputs someone imagined for it, and two leaf tests that each cover half
a condition still prove nothing about their conjunction.  The caller is what
supplies the combinations nobody enumerated.  Both times the caller was
worth more than another round on the leaf.

### 186. `initdigital` is the rate negotiation, and its own strings name it

`initdigital` (0x59980, 1206 bytes) became ready the moment `initV34` and
`preinitdigital` landed, because it is the thing that calls them.  Now
reconstructed and passing; this entry was written first, from the five
surviving debug strings that name most of what it computes and from the way
it confirms `initV34`'s signature from the caller's side.  Everything below
survived the differential test.

**It calls `initV34` twice, once per context**, which is the shape finding
181 predicted from the other direction:

```
   initV34(obj + 0x25e0, cfg[0x00], 2400 * cfg[0x04], ..., obj + 0x2a68, d)
   initV34(obj + 0x0a00, cfg[0x12], 2400 * cfg[0x14], ..., obj + 0x0e84, d)
```

`obj + 0x25e0` is the transmit context's fields and `obj + 0xa00` the
receive one's -- 0x1be0 apart, and each gets its own coefficient block.  The
`bitrate` argument is a genuine bit rate in bps, formed as 2400 times a
count, which is why `initV34` divides it by 25 rather than by anything
rate-like (finding 183): 2400/25 is 96, so the quotient counts bits per
symbol group directly.

**The author's names**, from `.rodata.str1.4`:

```
   0xd7f4  "V34DATARATE, for tx data rate - %d, PTC - %d, setting
            nofTxBits to %d\r\n"
   0xd83c  "V34DATARATE, finally txbitrate %d,rxbitrate %d\n"
   0xd86c  "V34DATARATE, preliminary txbitrate %d,rxbitrate %d\n"
   0xd8a0  "FATAL ERROR(initdigital) - ZERODIV expected!"
   0xd8d0  "--ERROR---, 2400bps is not possible at %d baud rate\n"
```

Matching them to the arguments pins four fields:

```
   cfg + 0x04   txbitrate, in units of 2400 bps
   cfg + 0x14   rxbitrate, same units
   obj + 0x08   PTC        (read)
   obj + 0x10   nofTxBits  = ((txbitrate * PTC) >> 6) + 6
```

where `cfg` is `obj + 0xaa84`.  "preliminary" is printed before the two
rates are clamped against each other and against a per-baud capability
bitmap at `obj + 0xaa0e`; "finally" after.

**ZERODIV confirms a field this tree already guessed.**  `v34shell.h` says
of `divisor` at +0xa42 that "zero means one", which was inferred from
`initV34` treating it as a width.  The string says the author thought the
same and called a zero there fatal: the code substitutes 1 and prints
"ZERODIV expected!" rather than dividing.  Two independent readings, and the
second is the author's own word.

**All five diagnostic call sites are carried**, per finding 134's policy,
and this is the first module in the tree whose test actually DRIVES them:
the sweep runs once with `dsplibs_debug_level` at 0 and once at 2, so the
gated branches and the register reloads inside them are compared rather than
merely present.  Everywhere else the level ships at zero and the branch is
never taken, which finding 134 notes is exactly why a missing call site
could go unnoticed for so long.

**Two things the reconstruction added to the map.**  The object is longer
than `struct v34_object` claimed: the declared end at 0xac10 was the largest
offset anything reconstructed had touched, and `initdigital` writes a byte
at 0xac16, so the struct now runs to 0xac18 and that is still a floor.  And
the rate config's `rx_baud` at +0xaa96 IS `faa96` -- one store, two
readings, the same situation as the echo array that is also the FSK delay
line (finding 100), declared in both places on purpose.

**One finding-129-class read.**  The transmit divisor lookup
`divtab[bits + 14*use_max - 1]` happens BEFORE the zero-rate test, so a zero
rate in mode 0 reads one entry before the table.  Unclamped and reproduced;
the fixture points the table at the middle of a scratch array so the read is
defined and identical on both sides rather than left to whatever follows.

### 187. The +0x14 buffers are the data path, and only the merge could tell

Two sessions named the same memory, and neither could have got it right
alone.  The V.34 handshake session read +0x0014..+0x021c as `scram_sink`,
`scram_src` and `scram_capture` -- "a scripted-bits loopback for the
object's own scrambler pair", i.e. a test harness.  The transmit session
read the identical five fields as `rx_data`, `tx_data`, `tx_rd`, `tx_n` and
`data_enable` -- the datapump's own buffers.

The two descriptions of the MECHANISM agree exactly: with the flag set the
scramblers draw their sixteen bits from one array and the descramblers
append to the other; with it clear the transmitter scrambles 0xffff and the
receiver discards.  What they disagree about is what that is for, and the
only evidence that separates them is who writes the flag:

```
   <scrambleGPC>:    cmpw   $0x0,0x2214(%eax)
   <scrambleGPA>:    cmpw   $0x0,0x2214(%eax)
   <descrambleGPC>:  cmpw   $0x0,0x2214(%esi)
   <descrambleGPA>:  cmpw   $0x0,0x2214(%edi)
   <preinitdigital>: mov    %ax,0x2214(%ebx)
   <modulatevector>: mov    %bp,0x2214(%esi)
```

Six accesses in the whole object, four of them reads.  `preinitdigital`
clears it; `modulatevector` SETS it, when the training-to-data symbol
counter passes the span in `fa00`.  So the object turns this on itself, at
the end of training -- which is the modem entering data mode, and settles it
as the data path rather than a facility something outside operates.

The handshake session saw only the clear, because `modulatevector` was the
other session's work and was not in its tree.  A reading that is right about
every instruction it can see can still be wrong about what the code is for,
and the missing evidence was one function away in a branch that had not been
merged yet.  Nothing about this was visible to either differential test:
both drove the same five fields through the same four functions and both
passed.

### 188. Four functions reconstructed twice, and the object settles the signature

The same merge found `scrambleGPC`, `scrambleGPA`, `descrambleGPC`,
`descrambleGPA`, `preinitdigital` and `Convolve16` written twice -- once in
`v34scram.c`/`v34digital.c` and once inside `v34shell.c`.  The two
reconstructions of each agree instruction for instruction; they differ only
in which base they measure from, the handshake session using the object
(`obj->scrambler` at +0x2a54) and the transmit session the transmit shell
context (`tx->scr[]`), which is the same memory 0x25e0 apart.

`preinitV34` was written twice under two names: the transmit session
reconstructed the object's own exported symbol, the handshake session an
identical private `preinit_shell`.  They are the same function.

WHAT THE OBJECT SETTLES.  `v34shell.h` had recorded the signature as open --
"`scrambleGPC` takes and returns a short where `v34_getbits_fn` uses int ...
which typedef is the original's is not settled".  It is settled:

```
   57860: sub    $0xc,%esp
   57863: mov    0x10(%esp),%eax          ; arg 1, the object
   57883: movswl 0x14(%esp),%edi          ; arg 2, SIGN-EXTENDED 16 bits
   578d1: cwtl                            ; and the return widened the same way
```

A parameter the callee reads with `movswl` is declared `short`, so
`short scrambleGPC(void *, short)` is the original's and `v34_getbits_fn`'s
`int (*)(void *, int)` is not.  The union at +0xe48 keeps all three
spellings, and the four callbacks are installed through the `scramble` one.

Kept: the dedicated modules, which carry the derivations the inline copies
do not -- GPA's four folds, and the observation that its `shr` could be
`sar` without any test being able to tell.  The blob's symbol order puts all
four between `getFrame` and `putFrame`, so `v34shell.c` is where the
original had them; that deviation is recorded at the head of `v34scram.c`
rather than acted on.

All three are now written; finding 157 is what they turned out to say, and
two claims in the paragraph above did not survive the reading.  **Both**
Initiate entry points pass mode 2 — `v34hshak.h`'s mode table says so and
this paragraph contradicted it.  And the timer group WAS corroborated, but by
`datapumpv34` and `v34handshak`, not by `VPcmV34SetV90RateReneg`: it writes
three of the four fields and leaves +0x244 alone.

### 189. The rate group is named, and the request entry points do not agree on what a request is

`VPcmV34InitiateHangUp` (258 B), `VPcmV34InitiateRateRenegotiation` (259 B)
and `VPcmV34SetV90RateReneg` (274 B) are written.  Finding 180 listed them
as the three that `v34handshakinit` unblocked; between them they name six
object fields that were pads and settle two things about the shape of the
V.34 object that the reconstruction had wrong.

**THE RATE GROUP.**  +0x220, +0x224, +0x228 and +0x22c are four `int`
INDICES — the same units +0xaa88 and +0xaa98 hold, which
`VPcmV34GetCurrentRxBitRate` multiplies by 2400 to answer in bits per
second.  Each name comes off a site:

```
  rate_min   +0x220   VPcmV34SetMinMaxBitRates divides a rate by 2400 into it
  rate_max   +0x224   ... and the other of the pair into this one
  rate_now   +0x228   v34handshak 0x63d03: both get (short)obj[0xaa98]
  rate_want  +0x22c   v34handshak 0x63395: read back, `js` to skip, then
                      compared with rate_now and adopted as obj[0xaa98]
```

Which way round min and max go is NOT settled by `VPcmV34SetMinMaxBitRates`,
whose fix-up raises +0x224 to +0x220 and reads the same either way.  It is
settled by the renegotiation: its step DOWN clamps at +0x220 and its step UP
clamps at +0x224.  And the `js` at 0x63395 is what says a negative
`rate_want` means "no target" rather than being an index — which is exactly
what the renegotiation's default arm writes.

**THE TWO RRN COUNTERS.**  +0xac0e and +0xac10.  These are the only fields in
`v34fsk.h` whose names come from a whole function rather than from a string:
`VPcmV34IndicateLocalRRN` is nothing but `+0xac0e++` and
`VPcmV34IndicateRemoteRRN` is nothing but `+0xac10++`.
`VPcmV34InitiateRateRenegotiation` does the first increment inline, which is
the same event counted at its source.

**"REQUEST" MEANS TWO DIFFERENT THINGS IN ONE FUNCTION.**  All three entry
points fork on `(unsigned)(status - 1) <= 1` — 1 and 2 mean a PCM receiver
has the line, the same test that picks the `V90Demodulator` branch in
`VPcmV34GetCurrentTxBitRate`.  On that arm the renegotiation forwards its
argument VERBATIM to `p3548 -> +0x175c -> +0x20c -> +0x8c`.  On the V.34
arm it reads only four values out of it: 0, 2 and 5 step down, 3 steps up,
anything else asks for nothing.  So the parameter is not one enumeration and
the header does not name it as though it were.

A step that would leave the bounds writes **nothing** rather than clamping —
`rate_want` keeps whatever it held, so a renegotiation asked for at the
bottom of the range still tears the handshake down and still counts, carrying
the previous request.

**THE SESSION OBJECT IS NOT WHAT `v34fsk.h` SAID.**  Its comment claimed
+0x6120 was "the only field of it any of them reads".  That was true of the
functions reconstructed when it was written and is not true of the object:
+0x610c, +0x611c, +0x612c, +0x6bd0, +0x1744 and +0x1760 are read as well, and
these two entry points walk a three-link chain through it.  +0x175c is the
`V90Demodulator` — `VPcmV34GetCurrentRxBitRate` hands exactly that field to
`V90Demodulator::getBitRate` — so the session object HOLDS the demodulator
rather than being it.  Corrected in place.  Same shape as findings 144 and
176: a negative claim about the object that was really a claim about what had
been read so far.

**Finding 179's +4 correction has two more witnesses.**  `datapumpv34` reads
+0x238 and +0x248 at their TRUE offsets (`mov 0x238(%ebx)` with `%ebx` the
object) and puts 5 in +0x4 when the difference passes 287,488; `v34handshak`
at 0x63cda copies +0x238 into +0x248 to restart the span.  Between them the
group is confirmed from outside VPcmV34Main.cpp's addressing habit.  +0x4
itself stays unnamed: three functions put 6 in it, `datapumpv34` puts 5 and
`VPcmV34Create` puts 10 and then 0, and nothing reconstructed reads it.

**`VPcmV34SetV90RateReneg` is `v34handshakinit`'s mode 2 with the state
machines left out** — the same four transmit-queue fields, the same
`& ~0x4018 | 0x2000`, the same `preinitdigital`, the same `f382` pair, the
same timer reset.  Two independent readings of one block, which is the
corroboration that the block was read right.  Its two parameters are both
tested for ZERO only: `cmp $1; sbb %edx,%edx` reads the *borrow*, so only
zero takes the low arm and a negative `rrn_type` takes the high one.  A
reconstruction that wrote `rrn_type < 1` would agree everywhere except on
negatives.  D48 records what it does to `v90_receiver`.

**WHAT THE TEST HAD TO GROW.**

*A three-link chain per side.*  The PCM arm reaches out of the object
entirely, so each side gets its own session, demodulator and leaf buffer,
prefilled with a pattern, and the chains are compared to each other
afterwards.  Comparing the leaf alone would not do: hang-up also sets a flag
byte at `p3548 + 0x173e` and the renegotiation does not, and that byte is the
whole difference between the two functions' PCM arms.

*Seeded pointers, for the reason last session learned the hard way.*  The
first fixture dereferenced `+0x0a28` straight out of the object under test.
Four mutants were "caught" by SEGFAULT rather than by a report — two of them
because the fixture crashed on the first case that diverged, which is a
defect in the test and not a diagnosis.  Seeding every skipped pointer with a
per-side dummy turns all four into clean failures.  This is the third time
that exact shape has appeared (`run_setupreceiver`, `check_self_ptr`, here),
so: **a fixture must never dereference a pointer it read out of the object
under test without seeding it first.**

*And the check that closes a gap the whole tree has.*  Every transcript
comparison in this tree raises the debug level first, so a call site that
lost its `if (DSPLIB_DEBUG_ON())` prints the same thing and passes — the
mutation survived.  The capture facility is independent of the level, so
turning capture ON with the level left DOWN tests the other half: below the
threshold these functions must say nothing.  Levels 0 **and 1**, because
every gate in the object is `> 1` and 1 is the only value separating it from
the `>= 1` a reader would write.  Both mutants are now caught.  The same
section would be worth having in every test file that compares a transcript.

**One thing the differential tier structurally cannot check.**  The step is
`dec`/`inc`, which wraps; C's signed `- 1` is undefined at the ends of the
range, and an optimiser may fold `rate_now + 1 <= rate_max` into `rate_now <
rate_max` on that basis.  Written as a plain `- 1` the file agrees with the
blob byte for byte at both extremes under this tree's compiler and flags --
the mutation SURVIVES -- so the agreement is a property of the code
generation and not of the language, and the one case that would break it is
the one case the test cannot see.  Spelled unsigned anyway; it costs a cast.

Adding the two extreme rows found a real defect, but in the FIXTURE: the
expectation helper had the same undefined `+ 1` and reported 41 where both
implementations produced INT_MAX.  An expectation computed by different rules
from the thing it is checking is only useful while the duplication is exact.

**Mutation:** 50 applied, 48 caught.  Both survivors are reorderings that are
equivalent: the flag byte at `p3548 + 0x173e` and the pointer load at +0x175c
do not alias, and `preinitdigital` does not touch +0x25c2 -- which the
surviving mutant is itself the proof of, since swapping the mask and the call
changes nothing in a whole-object comparison.  Four of the 48 were caught only
after the fixture stopped crashing, and two only after the silence section
existed; before those, both classes read as passes.

### 190. The mutation score was measuring the harness, not the tests

Running every mutation set after the merge produced numbers that did not
match what had been reported when each set was written: callprog 3 of 17,
v8hs 0 of 7, and five anchors across v8handshak and v8seq reading ANCHOR
MATCHES 0 TIMES.  Checked against the pre-merge commit, the numbers were
identical -- so nothing the merge did caused them, and the earlier reports
of "fully caught" were wrong.  Three separate causes, and only one of them
was a real gap:

**The harness ran the binary but not the gate.**  `make test` depends on
`strings`, which is the invented-string sweep; `mutate.py` ran only the test
binary.  Fourteen of callprog's seventeen mutations corrupt a format string,
and a corrupted string is exactly what a differential transcript CANNOT see
unless the test raises the debug level over the site -- which is finding
134's argument -- while the sweep rejects it immediately.  So the tree did
catch all seventeen and the harness was reporting on itself.  Fixed by
running `make strings` as part of the caught check.

**The suite-to-binary pairing was tribal knowledge.**  v8hs.json is caught
7 of 7 by `t_v8util` and 1 of 7 by `t_v8hs`, whose name it shares.
dialer_grading.json belongs to dialer.c and `t_dialer`, not to dialercfg.c.
Pointing a set at the wrong binary yields NOT CAUGHT for every mutation in
it, which is indistinguishable from the set being genuinely untested.  Fixed
with `test/mutations/suites.json` and `--suite`, so the pairing is stated
once rather than remembered.

**Five anchors were left behind by a rename.**  `v->mode` became `v->side`
in the V.8 batch; one anchor was repaired at the time and five were not.
An unusable mutation fails open -- it prints a line and is not counted as a
failure -- so a rename can silently retire a check, which is the same
failure mode as a dropped call site and deserves the same suspicion.

`tools/mutate.py --all` now runs the lot: 152 mutations over 14 suites, all
caught, nothing unusable.

AND IT REPORTS WHICH GATE FIRED, because a bare 152 of 152 would have made
the same mistake in the other direction.  138 are caught by the differential
tests -- this string, here, with these arguments.  14 are caught only by the
string sweep, which proves the literal is in the blob and NOTHING about
placement, arguments or whether the site ever fires.  All 14 are
CALLPROG_Progress's, which is finding 154 saying the same thing from the
other side: 29 sites placed there, 3 of them actually driven.  The two
numbers agree, and they only agree because the score stopped rounding a weak
check up to a strong one.

### 191. A merge is the one edit nothing in the tree could check

Every `/* +0xNNN */` in the headers is a claim about the object's layout, and
the padding between fields is written as an ABSOLUTE span -- `unmapped_a948
[0xaa0c - 0xa948]` -- so a wrong span slides every field after it and stops
only at the next hand-written assert.  Both spellings compile.

Interleaving two branches' fields into one struct means recomputing those
spans by hand, four times in this merge.  The hand-written `V34OB_ASSERT`
checks cover a few dozen fields, chosen next to the code that needed them:
the highest one in `struct v34_object` was at +0xaad0, and the last merge
spliced two field groups into the tail at +0xac0e, past everything asserted.
A two-byte error there would have compiled, passed every test, and been
inherited by whatever the next session built on it.

`tools/offcheck.py` generates one `offsetof` assertion per annotation and
compiles them: 864 of them, all matching, and `make offsets` now runs it as
part of `make test`.  Backdating a two-byte error into one padding array
fails 28 assertions, not 1 -- which is the propagation, made visible.

The twelve annotations it skips are `struct v8_v21_params`, which annotates
each field with where the STRUCT sits in the V.8 object rather than with the
field's own offset.  That convention is legitimate and the tool names it
rather than guessing.

### 192. gcov works here, and it names the 77 sites nothing runs

Three checks already look at the diagnostic call sites and none of them
answers whether a test ever REACHES one.  `--missing` counts sites in the
blob against sites in our source.  `--invented` checks the literal is the
original's.  `mutate.py` breaks a site and watches for red, but a mutation
caught by the string sweep rather than by a differential test has told you
the literal exists, which is the first check again.  Task #50 has been
closing that gap by hand, one function at a time.

`gcov` answers it directly, and the surprise is that it works at all.

**Instrumentation does not perturb the differential tier.**  `--coverage`
disables some optimisation, and with `-mfpmath=387` changed optimisation can
move x87 spill points and so change the rounding of intermediate values --
exactly what this tier exists to detect.  It does not happen: all 62 test
binaries pass instrumented, `t_v34ec` and `t_v34rx` included.  That is a
property of this tree's flags and worth re-checking if they change, not a
general licence.  The link needs `--coverage` in LDFLAGS as well as CFLAGS;
`build/dsplibs_ref.o` stays uninstrumented and does not care.

**Every binary accumulates into one set of counts.**  They link the same
objects and the .gcda path is baked in at compile time, so running all 62
gives suite-wide coverage with nothing to merge.  The tests set
`dsplibs_debug_level` per case and the transcript blocks already sweep 1 to
3, so a plain run reaches every site any test drives at any level -- there is
no level to configure and no second run to diff against.

**77 of 278 sites never execute.**  By file:

```
   callprog.c   30 of 33      v34rx.c      12 of 24
   cadence.c    17 of 24      v34info.c     4 of 26
   dialer.c      9 of 39      v34hshak.c    2 of 10
   fpm_div.c     1 of 1       v34shell.c    1 of 5
   fpm_mrf.c     1 of 1
```

THE RECONCILIATION IS THE REASON TO TRUST IT.  Finding 154 worked out by
hand, from the disassembly and from which mutations went red, that 3 of
`CALLPROG_Progress`'s sites were verified and the other 26 were not.  This
reports 3 live sites in callprog.c and 30 dead, having been told nothing
about any of it, and names the three: `request_state`'s STATE line and
`run_timeouts`' two timeouts.  Two methods with nothing in common agreeing
on a number that small is the strongest evidence either has produced.

It also confirms what the V.8 batch was for: src/v8 does not appear in the
list at all.  Every site placed there is driven.

**And it shows why line coverage alone would have been useless.**  The suite
covers 96.7% of executable lines in src/ -- 8548 of 8843 -- while 28% of the
diagnostic sites never run.  A gated call site is one or two lines out of
thousands; a percentage cannot see it.  The number that matters is the one
this counts, not the one the tool advertises.

`tools/debugcov.py`, ten seconds end to end.  Deliberately not wired into
`make phase` and deliberately not failing on a dead site: 77 of them exist
today, and a gate that is red from its first run is a gate somebody turns
off.  The number is there to be compared against the last one.

### 193. mewt found fourteen gaps in a file the hand-written mutations called done

`tools/mutate.py` was written here, and the question was whether to replace
it with something standard.  Spiked against **mewt 4.0.0** (Trail of Bits),
in a throwaway worktree.

IT WORKS, WITH NO ADAPTATION.  mewt's language list says C++, and the
tree-sitter C++ grammar parses this tree's C: `mewt mutate` on v8jm.c
produced **1219 mutants** where the hand-written set has 29.  Its config
model happens to be the same shape as `suites.json` -- a per-target glob with
its own test command -- so `test.cmd` is one line of shell.

IT IS ALSO CHEAP, which was the surprise.  One mutant costs a recompile of
one file, a relink and a test run: **0.2 seconds**.  The whole 1219 ran in
five minutes.  (mewt's own estimate said two days, having assumed a test
suite rather than a single fast binary.)

```
   1197 tested    898 caught    299 uncaught    22 skipped
   high 100.0%    medium 95.1%    low 65.9%
```

**High severity 100% is independent corroboration** of the hand-written work:
nothing mechanical could break v8jm.c in a way this tree does not notice.

**The 14 medium-severity survivors are real, and of two kinds.**  Eight are
`initTxSequence`'s field clears -- `jm->bitpos = 0`, `wordidx`, `repeats`,
`crc_enable`, `shifter`, `shifter0`, `nleft`, `nleft0` -- which delete
cleanly because the fixture hands in a buffer that is already zero.  A
statement whose whole job is to write a zero cannot be tested against a
zeroed fixture, and that is a property of every fixture in the tree that does
the same, not of this function.  The rest are unchecked outputs: `out->b0 |=
1` at the end of `rebuildJMSequence`, `out->b1 &= 0x3f`, the `out->b2`
bit-39 assembly, and `if (list[0] == 0) return 0` in the acceptance scan.
Nothing asserts on those bits.

WHY THIS IS NOT A MIGRATION.  The two tools answer different questions and
only one of them is mutation testing in the usual sense.  mewt generates
mechanically and finds gaps.  `mutate.py`'s entries are hand-authored CLAIMS
carrying the reading they test -- "swap CJ and JM in the message timeout",
"announce the caller's side as the answerer's" -- and each is tied to a
finding.  No generator can produce those, because none of them is a syntactic
transformation: they require knowing that this argument is the side and that
"CJ" is what the answerer waits for.  They are also the only thing that
exercises the `strings` gate and the by-test/by-strings split.  Deleting them
to adopt a generator would trade documentation of intent for volume.

So: keep both, and use mewt as a periodic sweep at `--severity high,medium`,
triaging each survivor into either a fixture fix or a new hand-written entry.
299 uncaught at low severity is more triage than it is worth on a schedule.

TWO CAVEATS.  mewt is AGPL-3.0.  That does not reach this tree -- it is a
tool run over the source, not linked into it, and the firewall in the
Makefile is about SpanDSP headers reaching `src/` -- but it is a reason to
keep it an external development dependency rather than vendoring it the way
SpanDSP is.  And it is a prebuilt binary from a GitHub release, so pin the
version and check the published sha256, which is what the spike did.

**DOES IT IMPROVE ON THE HAND-WRITTEN SET?**  For finding gaps, yes -- the
fourteen above.  For replacing them, no, and the operator list says why
rather than an argument:

```
   AAOS AOS AS BAOS BL BOS COS CR DAS ER IF IT LC LOS MR NR RDV SAOS SOS VR WF
```

**There is no string-literal operator.**  Nothing in mewt can turn `"CJ"`
into `"JM"`, `%x` into `%d`, or `"Got"` into `"Didn't get"`, and no operator
MOVES a statement, so nothing can express "announce the overflow before the
sample is clamped".  Those are most of what the hand-written entries are.
The overlap is `CR` (drop the announcement) and `AS` (swap two printf
arguments, and only if they are adjacent).  So the two sets are largely
disjoint by construction, not by accident: mewt mutates syntax, and a
reconstruction's claims about a debug transcript are claims about content and
position.

**DOES IT REPLACE THE DEBUG-PATH SWEEP?**  No.  Tested directly, since it is
the sort of thing that sounds true: `CR` alone over callprog.c, where
`debugcov` says 30 of 33 sites never execute.  184 mutants, 81 seconds, 85
uncaught -- **13 of the 30 dead sites**, and no false catches among them.

The 43% is structural, not a tuning problem.  `CR` replaces a STATEMENT, so
where a site shares an enclosing block or sits inside a larger construct,
one mutant covers several sites at once and the individual ones never get
their own.  The thirteen it does find arrive as thirteen rows among
eighty-five needing triage, with nothing marking them as the interesting
kind.  `debugcov` names all thirty, exactly, in six seconds for the whole
tree.

They are answering different questions again.  Mutation testing asks whether
a change would be noticed; coverage asks whether the line ran at all.  A dead
call site happens to be visible to both, which is why the question is worth
asking -- but the tool built for it is an order of magnitude faster and does
not miss more than half.

### 194. One transcript test drove 24 sites live, and found three placements wrong

`debugcov` said 77 sites never execute and 30 of them were in callprog.c, in
`CALLPROG_Create`, `_Delete` and `_Dial` -- functions two tests already CALL,
at level 0, where every gated site is unreachable.  Adding one level-1..3
transcript comparison to `t_callprog_create` took the tree from **77 dead
sites to 53**, and the sites it lit up were not all where we had put them.

**`CALLPROG Create <<` marks the EXIT.**  It was first in the function, on
the reasoning that its gate is at 0x79588 and the other at 0x795e6.  That
reasoning is wrong in general: GCC moves these blocks out of line, so the
order of the GATES is not the order they execute in.  The object prints it
immediately before `CALLPROG Dialing`, i.e. last.  The arrows meant what they
said all along -- ">>" going in to the nested create, "<<" coming back out of
this one -- and the source comment's unease about "the opposite of the
convention everywhere else" was the tell.

**`CALLPROG_Delete is entered` is printed after `DialerAbort`,** not before,
which is again the opposite of what the wording suggests.  A store cannot
cross a call, so store-vs-call order is readable from the object; a gated
print can sit either side of one and nothing but the trace can say which.

**`INTEGRATION_LENGTH` is announced before the interval is taken off.**  We
printed 4900 where the object prints 5000: the `validation -= 100` sat above
the report and belongs below it.  Invisible to every other check, because the
value that reaches `cfg` is the same either way -- which is exactly the class
of thing the whole-object comparison cannot see.

**And it found a missing site by itself.**  `toneiir_create`'s
`INTEGRATION_TIME = %d Buffers.` was on the audit's missing list and turned up
as a line the blob printed and we did not, in the middle of the cadence
report.  Placed between the division and the two counters, where the object's
cold block returns.

ONE THING ABOUT THE TEST ITSELF.  Comparing the raw capture failed for a
reason that had nothing to do with the modem: `runtime.c` writes a
`<< get_param N >>` marker into the transcript for every parameter read, and
the two sides do not read parameters in lockstep -- the reconstruction holds
some in a local where the object re-reads them.  `dsplib_debug_capture_lines`
already excludes those markers, which is why the line counts agreed while the
strings did not.  The comparison strips them; the object's own output is what
is being checked.
### 195. Six V.34 mutation sets, and what they said about the tests

V.34 is about two thirds of the reconstructed bytes and had no mutation sets
at all: every one of the fourteen was callprog, dialer, pulse or V.8.  The
work had been done — three times — in throwaway shell scripts deleted after
they printed their tallies, so finding 189's "50 applied, 48 caught" was a
number nobody could reproduce, extend, or re-run against changed code.

The scripts were still in the session scratchpad.  Recovering them made these
sets the original runs rather than a fresh derivation, which matters: a
different 50 would not have reproduced anything.

```
  v34pcmif   58   56 caught   2 equivalent   the three request entry points
  v34hshak   31   30 caught   1 equivalent   v34handshakinit and the trace
  v34filters 21   21 caught                  the echo canceller, the modulator
  v34info    33   32 caught   1 equivalent   the INFO0/INFO1a codecs
  v34shell   24   21 caught   3 equivalent   the rate negotiator
  v34rx      30   17 caught                  13 NOT CAUGHT, see below
```

The counts are higher than the originals because `sed s///` replaces on every
line it matches: one `s/rx->f258 = 0;//` hit both functions that clear it and
`0x2218` all three.  Split per function, they are also better mutations —
"hangup does not clear f258" and "reneg does not clear f258" are two claims.

**A HARNESS THAT REPORTS ON A STALE BUILD.**  `prologue txflags 0x7fff ->
0x3fff` was recorded SURVIVED and is caught.  The throwaway script restored
its source with `cp` after a build that had already failed, so that mutation
ran against the *previous* mutation's binary.  Finding 190 again, in the
tool that is supposed to detect exactly this.

**EQUIVALENT MUTANTS ARE NOW A KIND OF ENTRY.**  Seven of these provably
cannot fail — two reorderings of stores that do not alias, a mask bit OR-ed
straight back in, a four-bit mask on a value that only ever holds three, and
three register reloads after a call whose inputs nothing has written.
`"equivalent": true` runs them, expects the survival, and prints the argument
from `"why"`; one that gets CAUGHT fails the run, because then either the
argument is wrong or the code has moved out from under it.  Deleting them
would have left "nobody thought of this" and "we thought of this and it
cannot fail" looking identical in the file.

**WHAT THE SETS FOUND.**  Twenty-two mutations were NOT CAUGHT on the first
run.  Nine were fixed here and thirteen remain; none of the twenty-two was a
defect in the code.

*Three in v34filters* — ungating the empty coefficient report, dropping the
early return before the header, weakening the clean-up's gate to `>= 1`.
Every transcript comparison in `t_v34ec` raises the level first, so a lost
gate prints the same thing on both sides.  The silence section from
`t_v34pcmif` — capture ON, level DOWN, levels 0 and 1 — catches all three.
This is task #5's shape and it is now in three files.

*Three in v34info.*  One was the same missing silence section.  One was
provably equivalent.  The third is the interesting one: swapping the two
K56Flex announcements was caught by NEITHER tier.  Not by the test, because
`m[3] = 0x88 | (k56 ? 2 : 0)` pinned bit 3 — K56Flex's *enable* — on for
every iteration, so the "disabled by remote" arm never ran with the level up.
And not by the invented-string sweep, because that is a substring test:
**a truncation of a real string passes it.**  `debugaudit.py` says of itself
that it is a necessary condition only; this is what that costs.  Bit 3 and
bit 1 are now separate loop variables, as is the receiver state that was
sharing `k56` with them.

*Twelve in v34shell*, ten of them one cause: the test ran `initdigital` at
level 2 and threw away everything it printed.  Comparing the transcript
catches ten, including both rate reports with their arguments swapped.  The
last two were unreachable branches, and both were unreachable for a reason
the fixture chose: every entry of the divisor table was `k*53-400`, which is
never zero, so the author's ZERODIV guard had no case; and the object was
memset to zero, so the non-linear encoder's CLEAR arm had nothing to clear.
One zeroed table entry and one seeded field.

*Thirteen in v34rx, still open.*  Two classes.  `v34FreezeEcho` and
`V34SetupDemodulator` have no transcript compared anywhere, so their strings
and argument order are undriven — including the near/far echo report pair,
which differ in one word and dump different cancellers.  The rest need the
sweep to reach specific counts and thresholds: NEC adaptation stops at
0x4650, FEC starts at 0x2bc, the steps are every 45th sample, and the
renegotiation window is a two-sided test on a counter.  Task #6.

**THE SCORE.**  329 caught over 20 suites, of which 314 by the differential
tests and 15 by the string sweep alone; 13 not caught, 7 equivalent.  It was
152 over 14 suites.  The 13 are the honest part of that number: they were
untested before these sets existed too, and nothing said so.

### 196. The cross-references check out, and two more of them did not

Finding 191 said a merge is the one edit nothing in the tree could check, and
fixed the half `offcheck.py` covers: the compiler now holds every `/* +0xNNN */`
annotation. The other half is prose citing prose — `finding 171`, `see D48` —
renumbered by hand whenever a merge makes room for two sessions' findings.
`tools/refcheck.py` is that half. 732 references, and `make test` now fails
on one that resolves to nothing.

**THE CHEAP MODE IS NOT THE IMPORTANT ONE.**  A dangling reference is loud.
The failure that actually happens is a missed renumber, which still resolves
— to an entry about something else — and reads exactly like a correct
citation. `--since REV` is that check: a reference that sits in the same
sentence it did at REV, still citing the same number, whose number now has a
different title.

Run against the tree as it stood at merge 1088d6d, it reproduces the repair
of 7202ba8 and finds **two more that the hand analysis missed**, both now
fixed:

```
  findings.md:7994   "Corrected by finding 152"          -> 176
  findings.md:9458   "FINDING 155'S OFFSETS ARE WRONG"   -> 179
```

The second is the sharper one. `v34fsk.h` cited the same claim and WAS
repaired — 155 to 179, with a commit message noting it was "the timer
offsets, not cadence's gates". The identical citation four thousand lines up
in findings.md was not, because a hand sweep goes file by file and that file
had already been visited.

**TWO WAYS TO GET `--since` WRONG, both found by getting them wrong.**

*Match on file and number and it reports eleven correct references.* Both
sides of a merge append to `docs/findings.md`, so "this file cited 152 before
and cites 152 now" is true of two unrelated sentences that arrived from
opposite parents — and the one from the other parent gets judged against this
parent's numbering. Keying on 48 characters of surrounding text separates
them.

*Then compare that context literally and you lose the ones that matter most.*
"Finding 149's trap, and finding 152's" became "Finding 173's trap, and
finding 152's". The 149 was corrected; correcting it changed the context and
hid the 152 beside it, which was not. So numbers inside the window are
blanked. Correcting one reference must not conceal its neighbour.

**LINES ARE JOINED BEFORE MATCHING**, which is the difference between working
and appearing to. References wrap across comment lines — `findings 116b,\n *
123 and 171` — and a line-based scan reads `116b`, drops the rest, and
reports clean. Comment leaders are stripped by extension: `#` is a leader in
Python and a heading in markdown, and stripping it there would eat the
targets this same file is scanned for.

**WHAT IT DOES NOT COVER, AND WHY THAT IS A CHOICE.**  A citation with no
keyword is invisible: "finding 162, which corrects 158" cites 158 and this
sees 162. The 120a-121k narrative refers to its own sub-findings that way
throughout — about twenty. Matching bare `\d+[a-z]` instead takes `1u`, `0f`,
`02x`, `400s` and every printf width in the test suite; restricted to three
digits and a letter it still takes `837k` out of `P(k) = -21k^2 + 837k - 354`
in v34rx.c. This runs in `make test`, where a false positive is worse than a
miss, so the keyword stays required. Write the word and it is covered.

### 197. The silence check, everywhere, and what it says about drivers

Finding 189 built one section — capture ON, level DOWN — for `t_v34pcmif`,
on the grounds that every transcript comparison in the tree raises the debug
level before capturing, so a call site that lost its
`if (DSPLIB_DEBUG_ON())` prints the same text on both sides and passes.
This spreads it. Eleven files already had the check in another shape,
usually a `lines(1) == 0` assertion inside a level sweep, which is
equivalent and cheaper than a section of its own. Five did not, and every
one of the five was completely undriven:

```
  t_fpm_agc      4 gates    8 of 8 mutants survived   ->  0
  t_encode       2 gates    4 of 4                    ->  0
  t_dialercfg   16 gates   32 of 32                   ->  0
  t_v34hshak    10 gates   20 of 20                   ->  2
  t_v34rx       19 gates   38 of 38                   ->  2
```

**LEVEL 1, NOT LEVEL 0.** Every gate in the object is `> 1`, so 1 is the
single value at which it disagrees with the `>= 1` a reader would write. At
0 both spellings are silent and both mutants live. Level 0 is swept anyway
where it is free, but it is level 1 that does the work — and cadence's
second threshold at `> 2` (finding 155) is why each file's gates are read
before its levels are chosen rather than pasting one loop everywhere.

**DRIVING THE FUNCTION IS NOT DRIVING THE SITE**, which is the finding here.
The first attempt at `t_v34rx` was one new section calling each of the nine
gate-bearing functions once. It reached six gates of nineteen. `adaptecho`,
`modem_serrint`, `setInitialPhase`, `TimingV34` and `receiver` all print
from behind counters and error paths that their own sections build up over
120 to 200 iterations; a fresh call lands nowhere near them. What worked was
adding the level as another dimension of the loop that already exists —
reusing the setup instead of reproducing it — with the byte comparison
skipped below the threshold, since seven million comparisons per pass prove
nothing about strings that level 2 has not already proved.

**AND THREE SITES NOTHING HAD EVER DRIVEN.** `setInitialPhase` and
`TimingV34` never captured at all, so their diagnostics had never been
compared. `modem_serrint`'s three were unreachable from its own sweep:
`f354c` was pinned at 0 and `f25c` at 0x40, so the NEC start announcement —
which needs `count = f354c + 1` to WRAP to zero — the stop announcement at
0x4650, and the negative-lag error path were all out of reach. Two extra
loop dimensions, kept separate because two inputs swept from one variable
cannot be told apart, and all three are driven.

The same mistake in the fixture, for the third time in three files: the new
section seeded a receive queue's `count` and `ring` but not `rd`/`wr`, which
are pointers into it. `V34agc` follows them before printing anything, so the
whole test segfaulted rather than reporting. **A fixture must not
dereference a pointer it has not seeded** — findings 189 and 195 say this
about `t_v34pcmif` and `t_v34info`.

**WHAT IT COST THE MUTATION SCORE, WITHOUT MEANING TO.** v34rx's suite was
the tree's only red one at 13 uncaught (finding 195); it is now 8. Four fell
out of the coverage above. The fifth needed its own fix and is worth
recording: `v34FreezeEcho` reports the near canceller then the far one, and
with both sets of coefficients equal — which is what init leaves — dumping
`echo0` twice prints exactly what dumping `echo0` then `echo1` prints. Two
patterns, and the transposition is visible.

334 caught over 20 suites, 8 not caught, 7 equivalent. The 8 that remain are
off-by-one mutations on thresholds — 0x600 becomes 0x601, `<= 0x464f`
becomes `<`, the renegotiation window's two bounds — which need the sweep to
reach exact counter values. That is task #6 and it is not silence work.

The one gate left undriven in each of the two V.34 files is worth naming so
neither reads as an oversight. `v34hshak`'s is `preempindex`'s "index is 0"
arm, which D36 records as unreachable: `i` is 6 or more by the time the test
runs, so no input drives it. `v34rx`'s is `setInitialPhase`'s second divide
guard, which needs `polyValue(k2) + polyValue(k)` to come out zero — the
sweep reaches the first guard and not that one.

### 198. The collision was invisible because the checker used a dict

Findings collided at 146-156, at 146-151 and again at 192-194 -- three
merges, one cause: two sessions branch from the same tip, both allocate from
`max + 1`, both are right when they do it, and the merge puts two `### 192.`
in one file.

Nothing caught any of the three, and `refcheck.py` least of all, which is
the interesting part because catching this is what it is for.  `titles()`
built a dict keyed by number, so the second entry silently replaced the
first and every citation still resolved.  The tree read as consistent while
two different findings answered to one number -- the same shape as finding
196's misdirection, one level up: not a reference pointing at the wrong
entry, but a number owned by two.

So the default run now reports duplicates before it reports dangling, and
`make test` gates on it.  A collision cannot land quietly again; it goes red
in the merge that creates it.

TELLING A COLLISION FROM A LIST.  Numbered lists inside a finding use the
same markup -- "### 1. What six LSB actually costs" sits inside a finding in
the twenties and is not finding 1 -- and the heading level does not separate
them, because real entries use both `##` and `###`.  What separates them is
that entries climb and a list restarts: anything more than 20 below the
running high-water mark is a list item.  A real collision is a repeat of a
RECENT number, since the merge that causes it appends 192, 193, 194 after
192, 193, 194, so it lands inside the window while a list at 1..9 does not.

AND RESOLVING IT IS NOW ONE COMMAND.  `--renumber 192 195` moves the heading
and every citation in the same pass, which is the part that went wrong by
hand: six references were missed the first time and two the second.  It
refuses a number that is taken and names the next free one.  What it does
not do is decide WHICH side moves -- that is a judgement about which numbers
are already published, and the tool should not guess it.

### 199. mewt ignores its own per-target configuration

`mewtsweep.py` paired each source with the one test binary that covers it,
through the `[[per_target]]` array mewt's config template documents: a
`glob` and a dotted `test.cmd`.  A smoke run on one small file was still
going an hour later.

`ps` said why -- `sh -c make -s test`, an hour in, which is the GLOBAL
fallback and not any per-target rule.  Proved rather than inferred, because
the baseline is expected to use the global command and that alone would not
have settled it: with `[test] cmd = "true"` (always passes) and the
per-target `test.cmd = "false"` (always fails), all 32 tested mutants came
back UNCAUGHT.  The command that ran was the one that always passes.  mewt
4.0.0 accepts the array, does not complain, and does not use it.

THE TOOL'S OWN MISTAKE WAS WORSE THAN THE BUG.  The fallback was
`make -s test` with a comment saying nothing should reach it.  Everything
reached it: the whole 62-binary suite per mutant, ten seconds instead of two
tenths.  A fallback chosen to be harmless-if-wrong is a fallback that hides
being wrong; had it been `false`, the first run would have failed in a
second and said so.  The same shape as finding 190, where a suite pointed at
the wrong binary reported NOT CAUGHT for every mutation and looked exactly
like an untested claim.

Now one run per file: its own config, its own database, `[test] cmd` is that
file's command, and there is nothing to fall back to.

WHAT IS STILL WRONG.  Even so, v8agc.c's 315 mutants did not finish in ten
minutes, where the manual spike did 1219 in five.  That is seconds per
mutant against two tenths by hand, and the cold worktree explains the first
build and not the rest.  Unmeasured, so unfixed, and the tool says so at the
top: a full sweep is days at this rate rather than the hour it should be.

### 200. It was not slow, it was waiting: 88% of the sweep was one timeout

Finding 199 left the sweep "slower than it should be, and not yet measured".
Measured, it was not slow at all.

The numbers, in the order they settled it.  A test binary runs in 23 ms and
a whole mutant cycle -- recompile, relink, run -- in **146 ms** by hand.
Driving mewt directly over 32 mutants took **3690 ms wall for 33 command
invocations**, one per mutant plus the baseline: **115 ms each**, FASTER than
by hand and with no repeated work to find.  So neither the rebuild, nor the
sqlite write, nor the process spawn was the problem, and all three suspects
named in 199 were wrong.

What settled it was running the tool's own worktree for two minutes and
counting: **182 mutants tested before, 182 after**.  Not slow.  Stuck.

`while (acc <= 0x1fffffff) acc += acc;` is v8agc's normaliser, and `COS`
makes it `!=`, `AAOS` makes it `*=` and `%=`.  None of the three terminates.
Each cost the whole `test.timeout`, which this tool had set to 120 seconds --
so three hangers in one small file were 360 seconds against 46 of real work,
and the run was 88% waiting.  More were coming: 130 of the 315 were still
untested when the ten-minute cap killed it.

The timeout is now 10 s, still 65x the real command.  The same file
completes in **114 seconds** where it had not finished in 600.

NOTHING IS LOST BY SHORTENING IT.  A mutant that never returns is a mutant
the tests DETECT -- an infinite loop is a failure -- and mewt records the
timeout as its own outcome rather than as an escape.  The only thing a long
timeout buys is patience for a slow test, and the slowest here is 303 ms.

AND THE FINISHED RUN FOUND ONE.  `if (delta < 0) delta = -(short)delta;` in
`V8agc` deletes cleanly: nothing in the tree asserts on the absolute value of
that delta.  One real gap out of one small file, which is the rate finding
193 saw on v8jm.c and the reason to keep the sweep.

### 201. cadence_create announces a window it is about to refuse

Five sites were dead in `cadence_create`'s RINGBACK and CONGESTION arms, and
the reason was not that the arms were unreached: `t_cadence`'s case table
drives all four tones and both zero-window refusals.  It drives them at
level 0.  `opt_level` is reset to 0 before the create block runs, so every
announcement in the function compiled to a branch nobody took while the
arithmetic around it was checked 220 ways.

Running the same cases at levels 1 to 3 found the order wrong.  A
zero-window RING printed **two lines where the object printed eleven**: the
refusal -- free the filter, free the detector, return NULL -- sat above the
nine-line report, and in the object it sits below it.  The object announces
the whole configuration, type and filter and both windows and buffer and
level, and only then decides the window is unusable and throws it away.

Nothing else could have seen that.  Both sides return NULL, both free
everything, and the caller cannot tell the difference; the object comparison
and the allocation balance agree either way.  The transcript is the only
witness, which is the same shape as finding 194's three placements.

ONE DIVERGENCE IS LEFT AND IS PINNED RATHER THAN HIDDEN.  For tones past
RING the line counts agree and the content does not, so it is one field
inside that nine-line block and not a site.  `name` is clamped with `>`, so
4 and 9 both print INVALID here and the object disagrees somewhere in the
report.  The comparison runs for the four real tones -- which is where the
five dead sites were -- and skips past RING with the reason in the test,
rather than the check being dropped or the failure smoothed over.

37 of 279 sites dead, from 42.  Line coverage 97.0% to 97.2%.

### 202. DialerAbort's two sites, and the pattern is now the whole finding

`run_abort` already drove all sixteen combinations of the three fields
`DialerAbort` reads -- and drove them at level 0, so its two announcements
had never executed: the error return above progress state 10, and the one
that reports `LastPulseDigitDialed`.  Moving the level with the sweep drives
both, and this time the reconstruction agreed with the object first try.
1041 checks, no divergence.  37 dead sites to 35.

THE SHAPE HAS REPEATED FOUR TIMES NOW and is worth stating once rather than
rediscovering: **the sites were never unreachable.  The tests already called
the functions, with the right arguments, in the right states.  They called
them with the diagnostics off.**

  callprog.c  CALLPROG_Create/_Delete/_Dial, called by two tests   194
  cadence.c   all four tones and both refusals, swept 220 ways     201
  dialer.c    sixteen combinations of the three fields             here

In each case the fix was one loop -- run the existing cases again with
`dsplibs_debug_level` raised and the transcripts compared -- and in two of
the three that loop found placements wrong that nothing else could see.  A
gated call site is not tested by a test that reaches it; it is tested by a
test that reaches it with the gate open, and the difference is invisible
until something counts what executed.

WHAT IS LEFT IS NOT THAT SHAPE.  The 35 that remain need states the tests do
not reach at any level: `CALLPROG_Progress`'s 13 and `detect`'s 3 want tone
verdicts and buffer conditions the drivers do not produce, `pulse_digit` and
`DialerProgress`'s five want a pulse dial in flight, and v34rx's three want
the far echo canceller past count 0x2bc.  Those are input problems, not
level problems, and each will cost more than a loop.

`pulse_digit`'s is diagnosed here, since the diagnosis is most of the work.
Its site fires when `IsPulseDialerReady` answers NO, and that function
returns 1 unconditionally when `call_of(modem)` is null.  Every dialler test
sets `MDMPRM_DP_ADDR` to 0 -- deliberately, because `SetPulseMakeTime`
dereferences whatever it returns -- so the dialler is always told the line is
ready and the waiting branch cannot be reached at any debug level.  Driving
it needs a real `struct call` with a pulse in flight, which is the fixture
those tests were built to avoid.

And v34rx's three are not this task's: the far echo canceller past count
0x2bc is what the V34RX session is already working through, having found the
`echo1` divergence at iteration 144 that its own finding records.  They will
arrive with that work rather than being chased from here.
### 203. The far echo canceller counted from the wrong call

Closing v34rx's uncaught mutations (finding 195) meant seeding `f354c` — the
echo adaptation counter — near the boundaries the mutations move. That drove
the FAR canceller past count 0x2bc for the first time in this tree, and it
did not agree with the blob. The cause was a genuine defect in
`modem_serrint`, now fixed.

**IT HAD NEVER RUN.** `modem_serrint` adapts `echo1` only once `f354c >
0x2bc`, which is 700 calls. Its own section started the counter at 0 and ran
300 iterations, so the branch was dead in every test that has ever passed.
`echo0`'s equivalent starts at zero and was always covered; the two look
alike in the source and only one of them was ever executed. Nothing about a
green run said so.

**THE DEFECT.** `far_count` was computed from `count`, the counter AFTER its
increment. The object computes it from the value BEFORE:

```
    mov  0x354c(%ebx),%eax     ; %eax = f354c, the old count
    lea  0x1(%eax),%ebx        ; %ebx = count = old + 1
    mov  %ebx,0x354c(%edx)     ; f354c = count
    sub  $0x2bb,%eax           ; far_count = OLD - 0x2bb
```

and then splits the two cleanly: `%ebx` drives every near milestone
(`cmp $0x90,%ebx`, `cmp $0x7d0,%ebx`) and `%edi`, sign-extended from `%ax`,
every far one. So the far counter runs one behind the near one and every far
event lands one call later than the same near event would.

**HOW IT SHOWED.** With `f354c` seeded to 0x2bb the two objects agree for
143 calls and part at iteration 144, `f354c` = 843, offset +0x3552 — which
is `far_count == 0x90` under our arithmetic and 0x91 under the object's. The
tell was that `echo1_hist` was byte-identical on both sides throughout and
`taps` matched, so `far_energy` could not differ; instrumenting both alphas
showed ours reaching −179 at call 843 and the blob reaching the same −179 at
844. Not a wrong value — the right value, one call early. Everything
downstream followed: `f3552`, then `far_err`, then all 288 bytes of
`echo1_coeff` and `echo1_frac` through `V34EchoAdapt`, and through
`V34EchoFilter` the cancelled sample itself, which is why the visible symptom
was the low half of every receive-queue entry.

**WRITTEN AS THE OBJECT WRITES IT.** `prev - 0x2bb`, not `count - 0x2bc`.
The two are the same number and the second hides which counter it came
from, which is the only interesting thing about it. A mutation now asserts
the distinction — swapping `prev` for `count` is the defect this finding is
about, and it is caught.

**WHAT IT UNBLOCKED.** The two mutations parked on it are caught: the FEC
start announcement at `f354c == 0x2bc`, and the far step's offset. The suite
is 30 of 31, from 17 when the set was written.

**AND ONE STILL OPEN.** The S-S1 threshold `(short)err > 0x600` is moved by
one and nothing notices, because `err` never lands on 0x601. Instrumenting
the site says it is reached 400-odd times across the whole test with values
spanning 156..2406 — and a search over 400 ring offsets produced not one
sample in 0x5f8..0x608. The quantity is `(dr*dr + di*di) >> 14` on the
distance from a fixed constellation point, so it is quantised in a way that
steps over the band. Reaching it wants the equaliser driven to a chosen
output, not the input swept. Task #8.

### 204. The invented-string sweep accepted a truncation, and one had got in

`make strings` gates on `debugaudit.py --invented`, which asks whether each
of our string literals appears in the object's `.rodata` or `.data`. It
asked with a plain substring test, so any literal that is a PREFIX or
fragment of a real one passed — and a truncation is exactly what a
mis-transcribed format string looks like. Finding 195 caught it in the act:
replacing

```
  "VPcmV34Main: K56Flex enabled by remote, PCM type: local %d, remote %d ..."
```

with the same message cut short at "remote" was rejected by neither tier —
not by the differential test, whose fixture pinned the branch off, and not by
this sweep, because the short version is a prefix of the long one.

**THE SUBSTRING TEST WAS LOAD-BEARING, which is why it had survived.**
`our_strings` scanned line by line. C glues adjacent literals, and this tree
splits nearly every long message across source lines, so a message arrived
here as its FRAGMENTS — 142 of 592 "strings" were pieces of other strings.
Each fragment is a substring of the whole, so the loose test was the only
thing making them match. Tightening the comparison without gluing first
would have reported a hundred false positives; gluing without tightening
would have changed nothing. The two halves only work together.

`RUN` already separates adjacent literals with `\s*`, which spans newlines —
it was simply never given any, because it was applied per line. Scanning the
whole file instead (with preprocessor lines emptied rather than skipped, so
line numbers survive) gives 463 whole strings where there were 592 pieces.
The comparison is now `t + b"\0" in haystack`: our literal must be a whole
string of the object's, not a run of bytes inside one.

**AND IT FOUND ONE ON THE FIRST RUN.** `src/v8/v8dp.c` named its datapump
`.name = "v8"`. The blob has no bare `v8\0` anywhere; it has `V8\0`, sitting
immediately before `v8: delete...\n` in `.rodata.str1.1` — which is the
string our lowercase version had been matching as a prefix all along. The
other three pumps really are lower case:

```
  b103   standalone in the blob: yes
  call   standalone in the blob: yes
  v23    standalone in the blob: yes
  v8     standalone in the blob: NO      V8: yes
```

So the original names three of its four datapumps in lower case and the V.8
one in upper, and the reconstruction had regularised the odd one out. No
differential test could have seen it: `.name` is a field nothing this tree
drives ever prints.

That is the second time a sweep over EVERY literal rather than every call
site has paid for itself — finding 180 is the first — and the first time the
strictness of the comparison, rather than its coverage, was what mattered.

### 205. `--renumber` did the wrong thing on the case it was written for

Finding 198 built `refcheck.py --renumber` because moving a finding by hand
is what missed six references once and two the next time. It was never run
on a collision — and a collision is the only reason to reach for it.

Given two `### 195.` headings it moved the **first**, which is the
established entry rather than the newly merged one, and rewrote **every**
citation of 195 in the tree — including the ones inside the entry that kept
the number, which then pointed at an entry about something else. Exit status
0, one line of success. Measured on a reconstructed collision, not supposed.

**THE AMBIGUITY IS REAL AND NOT THE TOOL'S TO GUESS.** With two entries
answering to one number, a bare `finding 195` in some third file names both
of them and no rule recovers which was meant. So `--renumber` now refuses a
duplicated number outright, prints both titles, and says what to do instead.

What it can do is move one of them by POSITION: `--nth -1` takes the last,
which is where a merge appends, moves that heading, and rewrites only the
citations inside **that entry's own section** — a new finding's
self-references travel with it. Every other citation of the number is listed
rather than touched, because each one is a judgement:

```
  195 -> 250: the heading and 1 citation(s) inside its own section.

  Still naming 195 -- these belong to the entry that kept the number,
  or cannot be told from it.  Place them by hand:
      docs/findings.md:10674
      tools/debugaudit.py:287
      ...
```

It also refuses while conflict markers are present, since two sides of an
unresolved hunk are not a state anything can rewrite. The workflow that does
work — and the one this session used twice by hand — is: resolve taking
**both** sides, then `--nth`.

**AND THE TOOL BIT ME WHILE I FIXED IT.** Running `--renumber 195 250` on the
real tree to test the guard renumbered the real finding 195, and one of the
citations it rewrote was inside `refcheck.py`'s own docstring — which
`git checkout` could not undo, because that file was the one being edited.
A tool that rewrites the tree needs its guard before its capability, not
after; this is the second lesson in this file about running a mutating tool
against the working copy, `mutate.py`'s chdir being the first.

### 206. The last uncaught mutation, and why sweeping could never have found it

`receiver` gives up on the equaliser and restarts when the slicing error
passes a threshold — `if ((short)err > 0x600)` — and moving that bound by one
was the tree's last uncaught mutation. It is observable only when `err` is
exactly 0x601, and nothing had ever produced it: a search over 400 ring
offsets returned not one sample anywhere in 0x5f8..0x608. Finding 203 left
it open with the note that reaching it wanted the equaliser driven to a
chosen output rather than the input swept. That turned out to be right, and
three separate things stood in the way.

**THE SITE WAS NEVER REACHED AT ALL.** It is the else-branch of data mode:

```
    if (flags & V34_RX_FLAG_DATA)  { ...data... }
    else if (rx->f1c0 > 1)         { ...the reference generator, S-S1 here... }
```

Every attempt had `V34_RX_FLAG_DATA` set, because that is what the receiver's
other cases use. With it clear and `f1c0 = 4` the site is reached on every
call; with it set, never. Two whole sweeps measured nothing and reported
"no sample in the band", which was true and meaningless.

**THE INPUT IS NOT THE KNOB.** With the equaliser coefficients zero the
output is zero whatever the queue holds, and err sits at 639 — the distance
from the origin to the constellation point — for all 32767 ring amplitudes.
The knob is the coefficients.

**BUT NOT ALL OF THEM AT ONCE.** Driven together the 80 taps move `f208`
about twenty counts per unit step, and err past the band in jumps of a
dozen: 129..27714 with nothing inside. A single centre tap moves `f208` by a
fraction of a count and err then walks through every value.

**AND THE DELAY LINE HAS TO BE FULL.** `V34EqualizerUpdateDelayLine` runs on
odd `i` of the `f128` loop, so one call pushes two of the eighty entries.
On a fresh object the two land at taps 78 and 79 and the centre tap
multiplies zero — which is why an early attempt with a full re-init per
trial got err = 639 again and looked like a dead end. Forty calls fill the
line, and the receive queue must be refilled before every one of them or it
empties and the line fills with silence instead.

With all four right, err is smooth and monotonic in the tap either side of a
minimum at 2304, and the boundary is a measurement:

```
    tap 5891  ->  err 0x600     the bound is not crossed
    tap 5892  ->  err 0x601     the first value that crosses it
    tap 5894  ->  err 0x602
```

The test drives all three and asks the object, not the fixture, which way it
went: `f124` is zeroed by the restart and by nothing else on that path. Two
coverage assertions say the branch was taken for 0x601 and not for 0x600, so
if the arithmetic ever moves out from under those constants the section
fails rather than going quiet.

**343 of 343.** Every mutation in every suite is caught, seven of them
recorded as equivalent. The number is worth less than the four things above,
each of which was a sweep reporting a clean result for a reason that had
nothing to do with the code.

### 207. The dual-tone verdicts are CONFIRMED ones, and B is not a 2250 Hz tone

`Found 2100` and `Found 2250` are two of callprog's remaining dead sites, and
finding 194's attempt at them added 2100 Hz and 2250 Hz signal sources and
seeded them into every state.  Neither fired.  Reading `dualtone.c` says why,
and the guess in that finding -- "a level or a duration this input does not
have" -- was wrong twice over.

**They are the CONFIRMED verdicts, not the plain ones.**  `CALLPROG_Progress`
tests `r == 3` and `r == 5`, and those are `DUAL_TONE_A_CONFIRMED` and
`DUAL_TONE_B_CONFIRMED`.  Verdicts 2 and 4 are the same tones NOT yet held.
Confirmation needs `hold_a` to reach `DUAL_TONE_HOLD`, which is 0x500, and
the hold counter is RESET whenever the other branch wins or the energy drops
below `min_energy` -- so it is 1280 samples of UNBROKEN detection, not 1280
samples of tone.

**And B is the FSK band, though a 2250 Hz tone does feed it.**  `energy_b`
is `notch_energy(total, yc)` where `yc` is the signal with BOTH the 1800 and
2250 notches applied -- so it measures what those two notches REMOVED, and a
sine at either centre is removed and therefore counted.  An earlier revision
of this entry said the opposite, that a 2250 tone "goes into a notch at 2250
and is removed, which is the opposite of what it needs".  Being removed is
what makes `energy_b` large; the correction is recorded rather than quietly
edited because the wrong version was pushed.

WHAT THE NEXT ATTEMPT NEEDS.  For 3: 2100 Hz held unbroken past 1280 samples,
which the existing SIG_2100 source can do provided nothing resets the hold --
worth checking `min_energy` against the amplitude of 5000 first, since a
single dip below the floor restarts the count.  For 5: an FSK-band signal,
which is what the V.21 answer sequence is, not a tone at a notch centre.
`t_v23tx` already generates the real thing and is the obvious source.

### 208. Two causes eliminated for the dual-tone sites, and it is neither

Finding 207 named amplitude and signal choice as what the dual-tone verdicts
needed.  Both were tried and neither is the blocker.

**Amplitude was wrong and is not the cause.**  The callprog test drove its
tones at 5000 where `t_dualtone` measures the detector as answering at
12000.  Raised.  Sites still dead.

**The waveform was wrong and is not the cause either.**  The other signals in
that test come from a 64-entry table, which at 2100 Hz is 3.8 samples per
cycle -- harmonic-rich, and the notch removes the fundamental while the
harmonics survive, so no branch can take the 88% of total energy it needs.
Replaced with a real `sin()` at 12000, as `t_dualtone` does.  Sites still
dead.

Both changes are kept: they are right in themselves, they cost nothing, and
they remove two explanations from the next attempt's list.

**WHAT IS LEFT IS THE GATE, NOT THE SIGNAL.**  `Dual_TONE_detect` is called
only where `automode_table[cp->state] == 1`, and this test seeds every state
and still never reaches the call.  So either no state in this fixture's table
has automode set -- which finding 19 is about, the detectors that never
assert when configured through `CALLPROG_Create` -- or the states that do are
not among the ones the seeding can hold.  That is a question about the table
`build_state_machine` writes, not about what is fed to the line, and it
should be answered by reading the table the fixture actually produces before
any more signals are tried.

### 209. The messages were produced 147 times while the announcements stayed dead

`automode_table` is 1 for states 3, 4 and 5 and 0 everywhere else, so
`Dual_TONE_detect` runs in three states out of eleven.  Reading that was
supposed to explain why `Found 2100` and `Found 2250` never executed.  It
explained nothing, because the test seeds all eleven.

What settled it was already in the test's own output.  It counts every
message either side produced, and the tally said **16:109 17:38** --
`CALLPROG_DUALTONE_A` 109 times and `CALLPROG_DUALTONE_B` 38.  Those two
messages are set in the same `if` bodies as the two announcements.  So the
detector was reached, the verdicts fired, the messages flowed, and the
`printf` two lines above each of them never ran.

Which leaves one possibility, and it is the one from findings 194, 201 and
202 for the fourth time: the runs that drive the tones sit ABOVE the level
sweep and execute at level 0.  Moving three seeded states into the loop
drives both sites, and the transcripts agree with the object first try.

TWO ROUNDS WERE SPENT ON THE SIGNAL AND THE ANSWER WAS NEVER THERE.  Finding
207 read the detector and concluded the verdicts needed holding and a
different band; 208 raised the amplitude to 12000 and replaced a 3.8-sample
table sine with a real one.  Both were true observations about the detector
and neither mattered -- the tone had been detected all along.  The tell was
sitting in the test's message tally through both rounds, and the cheaper
question was never asked: not "why does the detector not fire" but "does
anything downstream of it show that it did".

Ask what the code AFTER the site recorded before theorising about the input
to it.  34 dead sites to 32.

### 210. A skip that outlived its reason, and what it was and was not costing

`t_callprog_progress`'s level sweep skipped `CPSTATE_DIALING` with a comment
saying every buffer there goes through `DialerProgress`, "28 call sites we
have not restored, and the transcript would diverge on those rather than on
anything here".  True when written.  Finding 162 restored those 28, and the
skip stayed.

Removed, and the sweep passes: the dialling state's transcript now agrees
with the object at levels 1, 2 and 3.

WHAT IT WAS NOT COSTING.  Nothing in the dead-site count -- still 32, and
coverage unmoved at 97.4%.  The state is driven at level 0 elsewhere in the
same test, so every site inside it had already executed and `debugcov` was
right to say so.  A skip in a transcript comparison hides an ASSERTION, not
an execution, and the two tools see different things: `debugcov` counts what
ran and cannot tell whether anything checked the result.

WHAT IT WAS COSTING is the check itself.  Eleven states minus one is a
comparison never made, on the busiest path in the file -- every dialled digit
goes through it -- and the only thing standing between that and a silent gap
was a sentence that had stopped being true.

Worth the habit: a skip carries the reason it was added, and the reason is
the thing to re-test.  This one was cheap to check and had been wrong for
several sessions.

### 211. Both DSP singletons were re-init and error paths nothing asked for

`fpm_div.c` and `fpm_mrf.c` had one dead site each, and both were the same
kind: a path the tests had no reason to take.

`FPM_div`'s is the divide-by-zero complaint.  Every caller in `t_fpm_div`
passes a real denominator -- the exhaustive sweep runs 196,608 checks and
not one of them is zero, because zero is not a divisor worth testing the
arithmetic of.  It is worth testing the announcement of.

`FPM_MRF_init`'s is the reallocate.  Every case in `t_fpm_mrf` re-inits at
the SAME size, deliberately, because the check that matters there is that
the buffer is REUSED rather than replaced.  Nothing grew `per_phase`
between two inits, so `history_len < per_phase` was never true.  Taps 20
then 40 over 4 branches takes it, and the object's message has no trailing
newline where every other one in that file does -- which is now checked
rather than merely commented.

Both agreed with the object first try.  31 dead sites to 30, and the two
files leave the list.

WHAT THIS SAYS ABOUT THE REST.  A test written to check arithmetic drives
the arguments arithmetic is interesting for, and a test written to check
reuse drives the case where reuse happens.  Neither is wrong; both leave the
complaint paths dark, and no amount of raising the debug level over the
existing cases would have reached them.  The remaining thirty are mostly of
that kind now -- inputs nobody had a reason to construct -- rather than the
gate-closed kind that four files' worth of one-loop fixes cleared out.

### 212. The DFT bin's spare int is two thresholds, and only its owner knows

*Written as 204 on the `v34hshak` branch and renumbered on merge: master had
taken 204 and 205 meanwhile.  Both numbers were quoted in a hand-over before
the merge, so anything citing "finding 204" for the DFT bin or "finding 205"
for the three machines means this one and 213.*

`struct v34_dftbin` is 0x2c bytes and `dftupdate`/`dftenergy` touch 0x28 of
them. The four at +0x28 were mapped as

```c
	int reserved;		/* +0x28  no reader or writer found      */
```

with the honest note that the field existed because the stride was measured
and not assumed. Reading V34hshak.c's five DFT leaves settles it: they are
two independent shorts, and the reason neither function in DFTC.c touches
them is that they do not belong to the bank — they belong to whoever owns
it.

`dftRetrainDetInit` writes 80 into +0x28 and 3000 into +0x2a on each of
three bins, and `detectRetrainReq` compares `energy` against +0x28 on one
arm of its state machine and against +0x2a on the other. So one is "this bin
has gone silent" and the other is "and now something is there", and the bank
carries them because the bank is what the comparison is about.

**THE TWO SIDES OF THAT COMPARISON ARE WIDENED DIFFERENTLY.** Every one of
the four sites is `movzwl` on the energy and `movswl` on the threshold:

```
   5e9ea:  movzwl 0xc(%ebx),%edx     ; energy, UNSIGNED
   5e9ee:  movswl 0x28(%ebx),%esi    ; threshold, SIGNED
   5e9f2:  cmp    %esi,%edx
```

Both sides are reachable, and getting to the energy one took two attempts.

The threshold side is easy: `dftRetrainDetInit` writes 80 and 3000, so
nothing but a seeded field is ever negative, and `t_v34hshak.c` seeds −1.
Read signed that rejects every energy, read unsigned it accepts every
energy, and the two answers differ on the first measurement.

The energy side looked unreachable. `dftenergy` writes `(short)((int)e >>
16)` where `e` is a sum of two squares of 16-bit values, so the short goes
negative only when `e` reaches 2^31 — both halves at full scale at once —
and a sweep of all 32767 amplitudes of a 1200 Hz sine gave no negative
energy at all. **That sweep held the wrong thing fixed.** A pure tone puts
almost all of its correlation on one axis, which is exactly the case that
cannot reach the corner; the sweep varied the one parameter least likely to
get there and the conclusion drawn from it — that the widening was
unobservable, recorded as an equivalent mutant — was wrong.

The accumulators are object fields. `0x04000000` in both of them is the
corner precisely: `<< 5` wraps to −2^31, `>> 16` gives −32768, and the two
squares sum to 2^31 on the nose. Silence adds nothing to an accumulator, so
a seed placed before the last update of a window survives into the
measurement, and with the threshold at 32767 the two widenings disagree
outright — −32768 is below it and 32768 is not. Four mutants that had been
filed as unable to fail are now caught.

Recorded because "no input I tried distinguishes them" is not the same claim
as "no input does", and the difference between the two was one fixed
waveform.

**AND THE FIVE ARE CALLED BY NOTHING.** Three scans, each with a positive
control, because the first two have both been wrong before in this project:

  - no relocation names `dftfreqinit`, `dftnlinitSignalBins`,
    `dftnlinitNoiseBins`, `dftRetrainDetInit` or `detectRetrainReq`;
  - no `call` **and no `jmp`** reaches any of them. The `jmp` half matters:
    a tail call carries no `call`, and this object is full of them —
    `VPcmV34ReportStartOfEchoAdapt` is nothing but one. The same scan finds
    six hits for `getbit` and `ApplyBulkDelay`, which is the control;
  - none of the five addresses appears as a little-endian word anywhere in
    `.text`, `.data` or `.rodata`, so nothing takes their address either.
    The control there is `0x629e0`, `v34handshak`'s default dispatch target,
    which appears 57 times in `.rodata`.

The second scan is the one that matters most, because a call to a symbol in
the same section can be resolved by the assembler with no relocation left —
which is how `getbit` and `ApplyBulkDelay` are reached from inside
`v34handshak` with nothing in the relocation table to show for it. The third
is findings 144 and 176 again: a section-symbol relocation with an addend
does not answer to a grep for the name.

Same shape as `cosread` and as four of the seven functions already in
v34hshak.c: global, testable, and with their arguments and bank sizes read
out of the code because no call site states them.

The bin numbers are the object's own `bin << 8`, and one bin is 150 Hz at
the 9600 Hz rate of R-1 — which is the V.34 line probe's tone spacing, and
`dftfreqinit`'s twenty-five bins are 150 Hz to 3750 Hz. The two `nl` banks
pair off exactly: signal at 1050, 1350, 1950, 2550 and noise at 900, 1200,
1800, 2400, each noise bin one step below its partner. That pairing is what
makes them a distortion measurement rather than eight frequencies. Which
bank is reference and which is product is **not** settled by the object, and
neither initialiser has a caller to settle it, so nothing here reads across.

### 213. `v34handshak` dispatches three machines, and finding 77 says one

*Written as 205 on the `v34hshak` branch; see the note on 212.*

Finding 77 read the three jump tables as "three tables over one state
variable, not three separate machines", on the evidence that `TX_L1` is
dispatched from two of them. The evidence is right and the conclusion is
wrong. The instruction before each `jmp *` says which word feeds it:

```
  62957:  movswl 0x3596(%esi),%eax   ; txstate
  62961:  cmp $0x51,%eax ; jmp *0x2da0(,%eax,4)    82 entries, cases  5..86

  62aea:  movzwl 0x3596(%edi),%ecx   ; txstate again
  62af7:  cmp $0x45,%eax ; jmp *0x2ee8(,%eax,4)    70 entries, cases  5..74

  64abc:  movzwl 0x3592(%eax),%esi   ; MICROSTATE
  64ac9:  cmp $0x27,%eax ; jmp *0x3000(,%eax,4)    40 entries, cases 41..80
```

Two tables over `txstate` and one over `microstate`. `rxstate` at +0x3594
has no table at all: it is dispatched by compare-and-branch chains, which is
why cfgsplit — which starts from dispatch targets — attributes every byte of
it to "shared by two or more". **The 12,290-byte "shared engine" of finding
77 is largely the receive machine**, and it is not shared machinery in the
sense that reading implies.

`TX_L1` appearing twice is then exactly what it looks like: state *value* 51
means one thing to the transmit machine and another to the microstate
machine, and `StateName` is one table indexed by all three.

What the three tables actually are:

```
  table 1  +0x2da0  txstate, INSIDE the per-sample transmit loop
                    25 real cases, 57 of 82 entries to the default
                    ~21.1 KB exclusive -- the signal generator
  table 2  +0x2ee8  txstate, once per block
                    16 cases over 7 distinct targets, ~0.3 KB
                    -- the transmit supervisor, deciding when to leave
  table 3  +0x3000  microstate, after fskdemodulate
                    16 real cases, ~27.6 KB exclusive
                    -- the protocol engine: DET_INFO 6046, DET_SYNC 3945,
                       TX_PHASE1_ANS 3198, RX_PHASE2_CALL 2385, ...
```

and the shape of the function around them, which the prologue gives in
twenty instructions:

```c
	while ((short)obj->txq.count < obj->f2aa0)
		switch (obj->txstate) { ... }		/* table 1 */
	if ((short)obj->rxq.count > 5)
		... rxstate, by compare chain, then table 3 ...
	else
		switch (obj->txstate) { ... }		/* table 2 */
```

**Why it matters for the plan.** "One machine with 87 states" and "three
machines sharing one name table" are different reconstructions and different
test strategies. The transmit generator can be driven by watching what lands
on the transmit queue; the protocol engine can only be driven by feeding it
a far end. Splitting the work by table rather than by state number is what
that distinction buys, and it is the split the task list now uses.

**The prerequisites, which are not optional.** Every test in this tree links
all of `$(OBJ)`, so one undefined symbol breaks the whole suite, and
`v34handshak` reaches 68 functions this tree has not written — about 34 KB
once its own 61.5 KB is set aside, of which 16 KB is the C++ half of
VPcmV34Main.cpp: `DILdescriptorPacker`, `V90Phase3Modulator`,
`V90PreFilter`, `VPcmFloModem`. That C++ is the real gate. The tree's
standing answer to a caller whose callees are missing is to not write the
caller — `CALLPROG_Progress`, `b103_process` and `FPM_iir_filt_block` are
all declined for exactly that reason — and this is the same answer at
sixty times the size.

### 214. The gate on `v34handshak` is in symmap.py, not in the object

Finding 213 established that `v34handshak` reaches 64 functions this tree
has not written — 33,406 bytes of prerequisites, 16,003 of it C++ from
`VPcmV34Main.cpp` — and concluded the function "cannot be committed in
pieces ahead of its callees", since `Makefile:121` links all of `$(OBJ)`
into every test binary and one undefined symbol breaks the whole suite,
not just the new test.

The link constraint is real.  The conclusion does not follow from it.

`symmap.py` renames **every** defined blob symbol to `ref_*`, so the blob
half of each test binary is self-contained and our half must supply
everything it calls.  That is a choice made when the harness was written
for leaf functions, where it costs nothing.  Rename only the symbols we
actually define, and the rest stay un-renamed — at which point an
unwritten callee resolves to the blob's own copy, **for both sides at
once**, and the caller links.

Spiked on branch `symmap-scaffold-spike`, commit `ea3b26d`: 384 renamed,
1966 left for the blob to supply, and **62 of 62 test binaries pass
unchanged**.  A program calling `probeselect` — 6173 bytes, not written —
links and runs.

#### Why sharing one physical copy is safe here

It is the `STATEFUL_IMPORTS` hazard one level down.  Both sides calling
the same `probeselect` is fine if it carries nothing between calls, and
catastrophic if it writes a blob global: our side's call would perturb
the blob side's next read, invisibly, while still producing a plausible
waveform.

A leaf audit is not enough — the hazard is transitive, because
`probeselect` calls things.  The closure of all 64 unwritten callees is
**111 functions, containing no store to `.bss` or `.data`**.  The one
flag is `edprintf` doing `movl $0x900,(%esp)` against `.bss`, which is
the *address* of a scratch buffer being pushed as an argument, not a
carried global.  This holds because V.34 state lives behind the
`tagV34Object` pointer, not in file scope; it is a property of this
object and would need re-checking against another.

#### What it costs, which is why it is a branch and not a merge

Today "the suite links" proves *everything reachable from what we have
written is written*.  That invariant is free and this spends it.  Without
a replacement, the tree can look far more finished than it is — against a
standing goal of a replacement that behaves identically to the blob, that
is the worst direction to be wrong in.

Anything landing this needs, at minimum: `make phase` printing the count
of blob-supplied symbols as a **ratchet that only ever shrinks**, and
`coverage.py` taught that a missing `ref_` alias now has a second cause
besides "file-local in the object".

It changes the **order**, not the total.  The finished tree still has all
33,406 bytes written.  What it removes is the sequencing rule that makes 16 KB
of V.90/V.92 C++ a precondition for touching the V.34 handshake at all.

### 215. The scaffold is not needed, and the closure is why

Finding 214 built a working escape hatch from the rule that a caller cannot
be committed ahead of its callees.  With V.90/V.92 confirmed as the project's
end goal rather than an optional extra, it should **not** be landed.  Four
reasons, in the order that decides it:

**The prerequisite set is bounded and small.**  64 functions, 33,406 bytes —
14 C at 17,403 and 50 C++ at 16,003 — and that is the *transitive* closure,
not a direct-callee count.  It bottoms out there.  `VPcmV34Main.cpp` is
312,776 bytes and this reaches 16,003 of them; the other 296 KB is not pulled
in.  Comparable in size to what the V.34 pass has already delivered.

**Nothing in it is a detour.**  The 16,003 C++ bytes *are* V.90/V.92 code —
the destination, not a tax on the way to it.  The 17,403 C bytes are V.34
handshake support.  Every byte is written in the end whichever order is
chosen, so the scaffold buys sequence, not scope.

**Each of the 64 is individually differentially testable today.**  They have
`ref_` aliases; the gate is on the caller alone.  The C ones are ordinary.
The C++ ones are methods, and the thing that could have made them
untestable — a `this` in `%ecx` needing real C++ to call — is not how this
object was built: `generateSymbol` passes `this` as the first *stack*
argument (`mov %eax,(%esp)`; `call`), plain cdecl, callable from C like
everything else here.  `reset` is a normal initialiser taking scalars and
pointers to small objects (`V90Jd` 140 bytes, `V92Jd` 216).

**The invariant is worth more as the tree fills, not less.**  Today "the
suite links" proves everything reachable from what we have written *is*
written.  Spending that at 15.9% translated, on a project whose goal is a
replacement that behaves identically to the blob, is the wrong direction to
be wrong in.

#### The caveat, which is real

Object sizes come from the largest `this`-relative displacement any method
uses, and two classes reach a long way: `V90Phase3Demodulator` 43,336 bytes
and `VPcmFloModem` 32,612.  Those are almost certainly reaching through into
an enclosing session object rather than being that large themselves, but it
means construction for those two may need the lifecycle
(`reset` → `enterPhase3` → use) rather than a zeroed buffer.  `V90PreFilter`
at 1,280 and `V90Phase3Modulator` at 916 are the tractable shape.

**If a prerequisite turns out to be untestable in isolation, that is when the
scaffold earns its cost.**  Branch `symmap-scaffold-spike` (`ea3b26d`) stays
for exactly that case — not merged, not deleted.  Finding 214 records the
mechanism and the safety audit, so nothing needs re-deriving.

#### One correction to how 214 was reached

214 presented the closure as though the direct-callee list might understate
it.  It does not: `callgraph.py --order` was already computing the transitive
closure, so re-deriving it found nothing new — it confirmed a number that was
already right.  What the re-derivation *did* catch is that a closure walked
from relocation targets misses file-local functions, whose calls relocate
against `.text+offset` rather than a name: `ApplyBulkDelay`, `getbit` and
`getMPrecvdBits` were absent from it and present in `callgraph`'s.  The
counts here are `callgraph`'s, and 214's "65" is corrected to 64 in place.

### 216. `V34SetupModulator`'s V.90 arm is live, and D31 quoted the wrong three instructions

D31 said the 3200-baud case's V.90 configuration could not be reached, on
this evidence:

```
   732f1:  ...                       ; entered via je from cmp $0xc80,%ebx
   732f7:  movl $0x1,0x4(%ecx)
   732fe:  movl $0x3,0x8(%ecx)
   73305:  je   7333e                ; <- always taken
```

`movl` sets no flags, so the `je` inherits ZF from the compare that selected
the case, which was equal — therefore always taken, therefore the arm is
dead. Every step of that is sound. **The listing is missing a line.** What
the object actually holds is

```
   732f1:  mov  0x40(%esp),%ecx
   732f5:  test %edi,%edi            ; <-- and %edi is the `v90` argument
   732f7:  movl $0x1,0x4(%ecx)
   732fe:  movl $0x3,0x8(%ecx)
   73305:  je   7333e                ; v90 == 0 -> the V.34 arm
```

`test` is the flag-setter and the two `movl`s were scheduled into the gap
between it and the branch, which is exactly the instruction ordering that
makes a `je` look orphaned. The prologue settles which parameter `%edi` is:
`sub $0x2c,%esp` after four pushes puts the arguments at `+0x40` (`m`),
`+0x44` (baud), `+0x48` (carrier), `+0x4c` (preemp index), `+0x50` (v90) and
`+0x54` (reset), and `72d57: mov 0x50(%esp),%edi` is the fifth.

So `v90` selects a whole configuration:

| | v90 == 0 | v90 != 0 |
|---|---|---|
| `taps` | 0x20 | **0x40** |
| shaping | `tx3200c1_for_v34` | **`tx3200c1_for_v90`** |
| pre-filter | `ec_prem_coef_B3200`/`High` by carrier | **`V90EchoPrefilterCoeff`** |
| `fc8c` | 15 | **14** |

and the carrier is consulted only on the V.34 arm. Two other claims fall
with it: `v34filters.c` said `v90` was "read by nothing in this function —
only printed", and the note on `fc8c` said every reachable site writes 15.

**Why no test caught it, which is the part worth keeping.** `t_v34ec.c` has
two sweeps over `V34SetupModulator`. The one that compares the modulator's
state drove `v90` at 0 for every case. The one that varies `v90` compares
only the transcript — and `v90` is printed on entry, before the switch, so
that comparison is identical on both arms by construction. Two sweeps, one
input, and the input was live in the sweep that could not see it and dead in
the sweep that could.

It surfaced from `v34setuptxmit`, which computes `v90` as "either PCM
receiver is running" and passes it straight in. Its own sweep crosses the
rates with the two receiver words, so 3200 baud with `v90_receiver` set is
an ordinary case there, and it failed on the first run.

**What is now tested.** `t_v34ec.c`'s state sweep takes `v90` as a
dimension, at 0, 1 and 2 — the object's test is `test %edi,%edi`, so 2
separates "non-zero" from a reconstruction comparing `== 1`.

Same shape as findings 130 and 171: two blocks that look alike, one input
that distinguishes them, and a fixture that held it fixed. The general rule
is the one this project keeps rediscovering — an argument driven from a
constant is an argument not tested — and the specific one is narrower:
**quoting a branch means quoting back to the last instruction that sets
flags, not to the last instruction that looks relevant.**

### 217. Ten of #59's seventeen are not available, and two different reasons say so

Finding 215 says every one of the 64 prerequisites "has a `ref_` alias and is
individually testable today", and the hand-over for #59 says "every one of
these has a `ref_` alias and is individually differentially testable today.
Nothing here is blocked." Neither is true of the whole list, and the two
things that make it untrue are different from each other and from the
scaffold question 215 settles.

Seven of the seventeen are available, and they are the whole of what is:
`chkForceBaudRate`, `GetVPcmMinimalTxPowerReduction`,
`VPcmV34GetMaxUpstreamRateIndex`, `VPcmV34InterpretMohMessageBits`,
`settxlevel`, `v34setuptxmit` and `probeselect`, about 8.7 KB of the 16.9.
**All seven are written** — `probeselect`, the largest at 6,173 bytes, in
finding 219. The other ten are below.

#### Three are file-local, so `objcopy` cannot give them a `ref_` alias

```
$ nm ../slmodemd/dsplibs.o | grep -wE 'getbit|ApplyBulkDelay'
0005dd10 t ApplyBulkDelay
0005eaf0 t getbit
$ nm ../slmodemd/dsplibs.o | grep getMPrecvdBits
00009250 t _Z14getMPrecvdBitsP12tagV34Object
```

Lower-case `t` is a local symbol. `symmap.py` emits a rename for every
symbol the blob *defines*, and `objcopy --redefine-syms` renames a local to
another local — so there is no `ref_getbit` for a test to call, and the tier-1
differential path does not exist for these three. `t_v34hshak.c`'s own header
has said the same thing about `StateName` since it was written, and about
`getbit` and `ApplyBulkDelay` in the paragraph beginning "`getbit` is NOT
here although it is unblocked".

**This is exactly the set 215's own correction paragraph names** — the three
a relocation-walked closure misses, because a call to a local in the same
section needs no relocation. The correction was right and the blanket claim
one paragraph later was written from the old list.

Total: 1,795 bytes with no tier-1 path under the current harness.

**There is a route, and it is not the scaffold.** `objcopy
--globalize-symbol=getbit` before `--redefine-syms` promotes the local, after
which the existing rename produces `ref_getbit`. It ADDS an alias; it does
not stop renaming anything, so the invariant 215 declines to spend — "the
suite links" proves everything reachable from what we have written is
written — is untouched. Each of the three names appears exactly once in the
symbol table, so globalising cannot collide. **Not landed**: it is a harness
change and those are the user's.

#### Seven reach something unwritten, and the link fails for both sides

```
$ python3 tools/symmap.py ../slmodemd/dsplibs.o -o /tmp/sm.txt
$ grep -E '_ZN12VPcmFloModem11enterPhase3Ev' /tmp/sm.txt
_ZN12VPcmFloModem11enterPhase3Ev ref__ZN12VPcmFloModem11enterPhase3Ev
```

`indicateJaTransmission` is 57 bytes and is nothing but two tail calls, to
`K56FlexFloModem::enterPhase3FullDuplex` and `VPcmFloModem::enterPhase3`.
Both are renamed in the reference object, so a C file defining
`indicateJaTransmission` leaves two undefined symbols and **all 62 test
binaries fail to link**, not just the new one. That is finding 214's premise,
and with 215 declining the scaffold it is the standing rule: a caller waits
for its callees.

| function | bytes | waits for |
|---|--:|---|
| `indicateJaTransmission` | 57 | `VPcmFloModem::enterPhase3`, `K56FlexFloModem::enterPhase3FullDuplex` |
| `V34GiveINFO1dBits` | 436 | `VPcmV34InitiateRetrain` (below) |
| `k56FlexPhase34` | 721 | `K56FlexFloModem::getK56FlexJaBits`, `::getK56FlexMpBits` |
| `datapumpv34` | 1028 | `v34handshak` (#56–#58), `VPcmV34IndicateLocalRRN`, `IndicateRemoteRRN` |
| `v90Phase34` | 1358 | `VPcmFloModem::getV90JaBits`, `::getV90CpBits` |
| `V34SetINFO1aBits` | 1401 | `VPcmFloModem::getUinfoValue` |
| `VPcmV34InitiateRetrain` | 1406 | four methods, incl. `V92EchoCanceller::setEchoDelay` |

Six of the seven wait on `VPcmV34Main.cpp`'s C++ half, which is **#60**. So
#60 is not merely "a scheduling question" as `fastpass.md` has it — it is a
hard predecessor of six of #59's functions, and that ordering was recorded
nowhere.

`datapumpv34` is the odd one: it waits on `v34handshak` itself, so it comes
after #56–#58 rather than after #60.

#### The counts do not agree, and both commands are given

215 says 14 C functions at 17,403 bytes. The list `callgraph.py` produces has
seventeen C-linkage entries, counting `getMPrecvdBits` — which is C++ by
mangling and C by everything else about it — and totals 16,889 bytes:

```
$ python3 tools/callgraph.py --order --of v34handshak
```

The three-byte-level difference is not chased here. What matters for
scheduling is the split, which both agree on: about 8.7 KB was writable,
1.8 KB needs a harness change, and 6.4 KB needs #60 or #56–#58 first.

### 218. `probeselect`'s shape, before it is reconstructed

*Superseded by finding 219, which reconstructs it. This map is kept because
219 records what it got right and wrong, and because everything below is still
the fastest way to read the function.*

6,173 bytes and the largest single thing in #59 — 71% of that task's available
bytes. **Not reconstructed when this was written, and not blocked either.** It has a `ref_` alias,
every callee it needs is written, and it is differentially testable today; it
was left out of this session for budget and nothing else. That distinction
matters, because everything else left out of #59 was left out for a reason
that will not go away by itself (finding 217) and this one will.

This is the map, written down so the next session starts from it and not from
1,354 lines of disassembly, and because two of the things below are claims
that want a differential test rather than another reading.

#### What it is

The line probe's verdict. It takes the object alone — `probeselect(obj)`,
one argument at `0x90(%esp)` after four pushes and `sub $0x7c` — and returns
nothing. Four cursors are set up in the prologue and used throughout:

```
  [esp+0x78]  obj + 0xa9ac   the outgoing MP message, ten shorts
  [esp+0x74]  obj + 0xaa84   the rate config  (V34_RATECFG)
  [esp+0x70]  obj + 0x264    the receiver
  edi         obj + 0xa320   the probe DFT bank, 25 bins
```

It opens by **zeroing ten shorts at `obj + 0xa9ac`** — the MP message it is
about to build — and ends by having written the rate config and that message.

#### It writes exactly the fields `setfinalrate` reads back

Every store into `[esp+0x74]` lands on a field `setfinalrate` already names:

| offset | absolute | `setfinalrate`'s name |
|---|---|---|
| +0x00 | 0xaa84 | `tx_baud` |
| +0x06 | 0xaa8a | `tx_preemp` |
| +0x0c | 0xaa90 | `tx_scale` |
| +0x10 | 0xaa94 | `tx_carrier` |
| +0x12 | 0xaa96 | `rx_baud` |
| +0x24 | 0xaaa8 | `rx_carrier` |
| +0x28 | 0xaaac | `rx_scale` |
| +0x2c | 0xaab0 | `rx_cdesc` |

and the MP bits it packs at `obj + 0xa9ac` are two bytes below `+0xa9de`,
`+0xa9e0` and `+0xa9e2` — the three `setfinalrate` unpacks its rate fields
out of. **So the two functions are an encode/decode pair over one message**,
which is the `--pairs` shape `callgraph.py` looks for and the only oracle in
V.34 so far that is independent of the blob. Whoever writes this should wire
that round-trip up: `probeselect` then `setfinalrate` must land on the rate
`probeselect` chose.

#### The five per-rate arms are one arm five times

Each recognised rate does the same nine things, differing only in constants:

```
  cfg->tx_baud    = <rate>
  cfg->tx_scale   = scale<rate>
  cfg->tx_carrier = <high or low, on one bit of the MP message>
  cfg->tx_preemp  = bitreverse(<a nibble of the MP message>, 4)
  cfg->rx_baud    = <rate>
  cfg->rx_carrier, rx_scale, rx_cdesc
  <OR some bits into the outgoing MP message>
  <the pre-emphasis search, below>
  cfg-><per-rate offset> = the index it found
```

with

| rate | cfg offset | search constant | search bin |
|---|---|---|---|
| 2400 | +0x16 | 0x7da7 | 18 |
| 2800 | +0x18 | 0x6789 | 18 |
| 3000 | +0x1c | 0x656f | 19 |
| 3200 | +0x1e | 0x639f | 20 |
| 3429 | +0x20 | 0x6626 | 22 |

#### The pre-emphasis search, and one branch that looks dead

Every arm runs this, with `ref` always `bins[4].energy`:

```
	x = bins[N].energy;
	i = 5;
	do {
		x = (short)((x * K) >> 14);
		i++;
	} while (x <= ref && i <= 9);
```

`i` is incremented **before** the comparison, at `lea 0x1(%ebx),%esi;
movswl %si,%ebx` ahead of `cmp %cx,%dx`. So `i` is 6..10 when either exit is
taken — and every arm's exit block then asks `cmp $0x5,%bx; je <index 0>`,
which **cannot be true**. The string on that path is
`V34PREEMPHASIS, - index is 0, baudrate= %d`, one of three; the other two,
`index is 10` and `index is %d`, are both reachable.

That is a reading, not a measurement, and it is exactly the shape D31 got
wrong — a branch declared dead from the instructions around it. Do not enter
it as a deviation until a differential test has driven the arm. If it holds
it is a real one: a whole index of the pre-emphasis range is unreachable.

**Index 10 is reached by BOTH exits and they print different strings.** The
`jg` taken on the iteration that makes `i` ten, and the `cmp $0x9`
fall-through, both leave 10 in the rate config — but the first reports it as
"index is %d" and the second as "index is 10". So the two are
indistinguishable in state and distinguishable only in the transcript, which
is findings 171 and 212's situation again, and the MOH decoder's two pairs of
near-duplicate arms in finding 214's neighbourhood. Compare the transcript.

**THE BINS MUST BE SEEDED NEGATIVE AS WELL AS POSITIVE.** Both `ref` and `x`
are `movswl`, and finding 212 established that a bin's `energy` really does go
negative — from a seeded accumulator, at `0x04000000` in both halves. A
negative `x` RISES towards zero under a multiplier below one, so it crosses a
negative `ref` from the other side entirely. A fixture with non-negative
energies never runs that branch.

**There is no 2743 arm.** Five rates are recognised and V.34's second lowest
is not one of them — which is the same fact as `chkForceBaudRate`'s `allow[0]`
and `allow[1]` being written and never read. Two functions, one gap, and
neither says why.

#### Two guarded arms a zeroed fixture never reaches

- `div %ebp` at 0x60de0 with `ebp = obj->0xaac4`, guarded by `test %ebp,%ebp;
  je 60df0`. A zeroed object always skips it, so the SNR ratio it computes is
  never exercised.
- the bit scan over `obj->0xaac8` at 0x60db0, with a bounded early-out at
  0x61a6f (`cmp $0x9,%bx; jle`) that takes a different division path.

Both sides of both need driving.

#### Both `chkForceBaudRate` call sites

0x6117a and 0x614b8, and they are not interchangeable: each is preceded by
its own loop over bins 16..23, one subtracting 2 from `shift` and one
subtracting 1, chosen by how far the summed band-edge energies missed. The
two say so — "so reducing band edge norm by 2" and "by 1". Reaching only one
is the easy failure.

#### The fifteen strings, which narrate the whole function

```
  V34PROBE, snr_L1=%d , snr_L2=%d , L2toL1ratio=%d  (all not in dB)
  V34PROBE, agc gainestimate of L1 signal is %d
  V34PROBE, dBcnt=%d , powerReductionReq=%d , gain=%d
  V34PROBE, asking for a power reduction of %d
  V34PROBE, not asking for power reduction
  V34PROBE, agc gainestimate due to power reduction request is %d
  V34PROBE, rx->gain=%d ,(obj->rxinfo0.data[1]&0x80)=%d
  Sensitive RX Power Reduction mechanism enabled! / disabled!
  V34PROBE, min = %d, i= %d, so reducing band edge norm by 2 / by 1
  V34PROBE,0=%d,...,24=%d          (all 25 bins' `shift`, in one line)
  V34PREEMPHASIS, - index is 0 / 10 / %d, baudrate= %d
```

`rx->gain` is the receiver's `agc_gain` at +0x136 and `obj->rxinfo0.data[1]`
is `obj + 0xa97e`, so two more field names come out of this function when it
is written.

### 219. `probeselect` is written, and the mutations did all the work

Finding 218 mapped `probeselect` without reconstructing it. It is now
reconstructed and passing, and this records what the map got right, the one
thing it got wrong, and — the part worth keeping — that **the differential
test agreed with the blob at every stage while the tests were the thing that
was weak**. A green run never once said the inputs were only touching one arm
of a threshold.

#### The map held, except for one register

Every structural claim in 218 survived: the encode/decode pairing with
`setfinalrate`, the five per-rate arms with their constants, the two guarded
arms, both `chkForceBaudRate` call sites and the pre-emphasis search's shape.

**The one error was a register's provenance.** 218 said the 2800 originating
arm tests `bins[22]`; it tests `bins[20]`. The comparison is `cmp $0x5,%ax`
at 0x61d24, and `%eax` was last loaded at 0x616cb — the top of the 2800
band-edge test — not at 0x61643 where the 3000 arm's copy comes from. Two
arms that read identically and read different bins.

The differential test found it in one run at exactly one byte: `+0xa9b2`,
which is `msg[3]`, bit 0. 336 of 28,185 checks failed and every one was that
byte. A check on "the rate it chose" would have passed; the rate was right.

#### Five sweeps, 18 survivors, then 0

| run | caught | uncaught | what the survivors were |
|---|--:|--:|---|
| 1 | 186 | 18 | two hangs of my own making, then the fixture |
| 2 | 198 | 5 | exact equalities |
| 3 | 202 | 1 | solved for the wrong constraint |
| 4 | 202 | 1 | same |
| 5 | **203** | **0** | 6 equivalent, with reasons |

**Round 1's real lesson was about the tool, not the code.** Two of the
mutations turn a loop into an endless one, `mutate.py` had no timeout, and it
prints nothing until it finishes — so a hung binary looked exactly like a slow
build and sat for sixty-two minutes. Killing it made that worse: SIGTERM does
not run Python's `finally`, so the restore never happened and the tree was
left carrying a mutation forty lines from the function being examined. A clean
rebuild hung the same way, which is what made it look environmental. Both are
fixed in `tools/mutate.py`: every run is bounded, a timeout is reported as
caught-but-by-the-clock, and SIGTERM is turned into an exception so the
restore always runs.

**Round 1's sixteen real survivors were one flaw.** `probe_next` yields 24
bits, so a uniform draw is never negative and never small: `snr_l1 <= 0x1f3`
had a chance of one in thirty thousand per case and `snr_l2 < 0` had none at
all. That single fact killed every threshold in the power section, both
sensitive-ISP arms, the bit scan's top half and the minimum search's starting
value — which is exactly the shape of the survivor list. Fixed by drawing each
scalar from a pool of its own boundaries three times in four.

**Rounds 2 and 3 were equalities, which sampling cannot reach.** `x > ref`
against `x >= ref`, and `t < ratio` against `t <= ratio`, are single points in
a 32-bit space. Both are constructible:

  - `ratio` is controllable. With `snr_l2` below 0x200000 the bit scan runs
    out, the shift is ten, and the quotient reduces to `((snr_l2 << 10) +
    (snr_l1 >> 1)) / snr_l1` — so `snr_l1 = 0x400` makes the ratio EQUAL
    `snr_l2`. The sweep walks the dB ladder's own recurrence and drives the
    ratio to each term and its neighbours.
  - the search's pre-image is solvable: the step is `(x * k) >> 14`, so the
    value landing exactly on a chosen reference is `(ref << 14) / k`.

**Rounds 3 and 4 are the same survivor, and the interesting one.** The gain
reciprocal's rounding term differs by ONE, and to be observable it has to
survive `* dbcnt >> 14` AND the `req > 7` clamp four lines later. Those pull
in opposite directions: the first wants a large `dbcnt`, the second a small
`req`. I solved it at the `dbcnt` of 1000 the short-circuit produces, which is
arithmetically correct and useless — every gain below 0x1000 then gives a
`req` in the hundreds and both sides clamp flat to 7. Re-solving with the
clamp in the search gives exactly nineteen gains below 0x1000 that work, each
at one specific ladder step, and seven are now in the pool.

**It is not enough to construct an input that makes a mutation arithmetically
observable. It has to stay observable through everything downstream.** That is
the sharper form of the rule and it cost two rounds.

#### D53 is confirmed by the compiler

218 flagged `cmp $0x5,%bx` as unsatisfiable and declined to file it, because
that is the shape D31 got wrong. Two independent measurements now agree: the
sweep asserts indices 6..10 were all returned and 0..5 never were, and `gcov`
reports the `i == 5` test executed 6,938 times with its whole body marked NOT
EXECUTABLE — GCC proved the same thing and folded it away.

**A claim I nearly published and did not.** I expected the site to show up in
`debugcov`'s dead list and drafted that as evidence. It does not: gcov marks
an eliminated body non-executable rather than executed-zero-times, so that
count says nothing here in either direction. The 30-to-31-and-back movement in
`make phase` over this session was a different site entirely — the
sensitive-ISP request, which the old fixture never reached and the fixed one
does.

### 220. Context is cumulative output, so the wall is a turn count

*Written as 216 and renumbered on merge: a parallel session took 216
through 219 for the `V34SetupModulator` / `#59` chain.  Anything citing
"finding 216" for the turn-count measurement means this one.*

Functions over about 6 KB have repeatedly cost a whole session without being
finished, and the obvious explanation -- the disassembly does not fit -- is
wrong.  Measured: `probeselect` at 6,173 bytes is 1,523 instruction lines and
about 14,500 tokens for one clean read, and `v34handshak`, the largest
function in the object at 61,541 bytes, is about 131,000.  Both fit a 1 M
window.  Nor is it tool output: across one 1,826-turn session every
disassembly tool together -- `dis.py`, `objdump`, `cfgsplit`, `relocscan`,
`readelf`, `callgraph` -- produced 153 KB, 12% of all tool results.

The session transcripts record token usage, so this can be measured rather
than argued.  Session `abff4cf1`, which reached the wall:

| turn | context | cumulative output | ratio |
|--:|--:|--:|--:|
| 25 | 69,945 | 12,453 | 5.62 |
| 100 | 143,961 | 86,842 | 1.66 |
| 300 | 359,920 | 281,435 | 1.28 |
| 508 | 623,348 | 579,161 | **1.08** |

**The ratio converges to 1.**  Past the fixed cost of the first few turns,
context growth *is* output growth.  Output runs 1,100-1,800 tokens per turn
across every session measured, which puts the limit near 500-900 turns
whatever the turns are about; the five sessions that reached it had 400-600
turns, and the ones that finished had 10-25.  It is a turn count wall, not a
size wall, which means the structural fix is a subagent -- its turns do not
accumulate in the parent -- and everything else is a constant factor.

#### The constant factor is still worth having

I first wrote this finding claiming the cause was the following, which is a
contributor and not the mechanism.  Recorded as written, because the
measurement above is what should have come first:

`diff_eq_int(fmt, got, want, input)` passes `input` to `fmt`.  Of the 1,667
call sites in the suite, **819 give a format string with no conversion in
it**, so the argument is formatted by nothing and silently discarded.  The
commonest shape is the whole-object byte compare, open-coded in several
tests:

```c
for (i = 0; i < (int)sizeof(da); i++)
        diff_eq_int("decoder state", ((unsigned char *)&da)[i],
                    ((unsigned char *)&db)[i], i);        /* t_v34rx.c:187 */
```

which computes the offset, hands it over, and prints `decoder state  got 68,
reference 136` six times without once saying which byte.  The failure knew
which field diverged and threw it away at the point of printing; the session
then earns that back by re-reading the disassembly, which is the loop that
consumes the context.

Nothing was wrong with the checks themselves -- every one of those 819 is a
real comparison that really passes.  The suite is not weaker than it looked.
It is that a *failing* one says almost nothing, and the cost of that only
appears on the large functions, where failures are many and the code to
re-read is long.

#### The fix is five lines and touches no call site

```c
if (strchr(fmt, '%') == NULL)
        fprintf(stderr, " [input %ld]", input);
```

Appending the input when the format did not consume it repairs all 819 at
once.  `make phase` is green with it.

#### And the byte loop should not be open-coded at all

`diff_eq_obj(what, type, a, b, input)` does what those loops do, and three
things they do not: it coalesces differing bytes into runs (one wrong 32-bit
accumulator is one report, not four), it counts one check per object rather
than 1,948, and it reports the first difference first because everything
after it is consequence.  `tools/whichfield.py` then turns the offset into a
field path out of the DWARF our own objects already carry.

Where the offset lands in a `pad_*` region, that is the answer: the
divergence is somewhere the reconstruction has not modelled as fields yet.

Spiked on branch `spike-objdiff`.  The ordered set of remedies, including the
per-dispatch-case testing that would make `v34handshak`'s 16 microstate cases
independently committable, is in `docs/largefunctions.md`.

#### A trap found on the way

`tools/dis.py` shadows the standard library's `dis`, which `inspect` imports.
Any tool in `tools/` that reaches for something substantial -- pyelftools, in
this case -- dies with `AttributeError: module 'dis' has no attribute
'COMPILER_FLAG_NAMES'`, naming neither the directory nor the file
responsible.  `whichfield.py` drops its own directory from `sys.path` before
importing rather than renaming `dis.py`, whose name is right and which
several findings cite.

### 221. File-local symbols were aliasable all along

`tools/symmap.py` and `tools/coverage.py` both stated, in their docstrings and
in the report itself, that a symbol the object keeps file-local "can never be
differentially tested directly, because objcopy cannot rename them and there
is nothing to link against".

objcopy can.  `--globalize-symbols` promotes them in one pass and
`--redefine-syms` then sees ordinary globals in a second:

```
objcopy --globalize-symbols=globals.txt dsplibs.o glob.o
objcopy --redefine-syms=symmap.txt       glob.o    dsplibs_ref.o
```

```
$ nm build/dsplibs_ref.o | grep -E 'ref_(call_run|b103_process)$'
00002e80 T ref_call_run
000055f0 T ref_b103_process
```

**241 symbols, 18,566 bytes**, written off on a claim nobody had tested.
Fifteen are functions this tree has already reconstructed -- 6,192 bytes --
and four of those are `call_run`, `b103_process`, `v23_process` and
`v8_process`, the top-level entry points of four datapumps.  Each has been
verified only through whatever a caller happened to expose.

Ten names resist, and the reason is worth keeping: `AGCv23_CFG`, `PROTOCOL`,
`TONEv23_CFG`, `ToneLPF`, `V23_AGC_DEF_ALPHA`, `V23_AGC_DEF_BETA`,
`rx_out_internal`, `sqrt_table`, `temp.0` and `tx_in_internal` each occur in
more than one translation unit.  Two statics of the same name are two
different objects; globalizing both makes one symbol and the link picks
whichever it sees first.  `symmap.py` counts on the raw list rather than a
deduplicated one -- counting a set finds nothing, which is the shape of check
that reports clean because it cannot fail -- excludes them, and names them
every run.

`make phase` is green with the two-pass object, so nothing depended on those
symbols staying local.

#### What is still wrong, said out loud

`coverage.py` has not caught up: it still files all 241 under "reached through
a caller instead", so the `tested` denominator is smaller than the truth and
the percentage flatters.  Task #62 covers that and the fifteen tests.  **The
headline `tested` figure should FALL when it lands**, and that is the number
becoming honest rather than a regression.

#### The general lesson, which this tree keeps relearning

The claim had been in two tools' docstrings long enough to read as settled,
and it was load-bearing: it decided that 18.5 KB of the object was permanently
out of reach, and coverage.py's headline number was computed on that basis.
Nobody had run the two-line experiment.  Compare finding 199 -- a fallback
chosen to be harmless-if-wrong is a fallback that hides being wrong -- and
finding 198, where a checker used a dict and so could not see a collision.

### 222. The intermediate object was counted as ours, and coverage read 98%

Finding 221's two-pass rename leaves an intermediate behind:

```
objcopy --globalize-symbols=globals.txt dsplibs.o dsplibs_glob.o
objcopy --redefine-syms=symmap.txt      dsplibs_glob.o  dsplibs_ref.o
```

`build/dsplibs_glob.o` is the whole blob with every one of its file-local
symbols promoted to global.  `coverage.py`'s `our_symbols()` walked all of
`build/`, skipping only `dsplibs_ref.o`, **by name**:

```python
if not name.endswith(".o") or name == "dsplibs_ref.o":
    continue
```

So it read the intermediate as our own compiler output and counted all 1,782
of the blob's symbols as reconstructed.  The report, and the `docs/coverage.md`
committed with d915b73, therefore said:

```
  translated  [#################################.]  98.0%    713824 bytes, 1782 symbols
  tested      [######............................]  17.5%    121351 bytes, 282 of 1727 that can be
```

for a tree that has reconstructed 290 symbols and 124,247 bytes.  Both figures
were wrong, in opposite directions and for the same reason: `translated` took
the blob for ours, and `tested`'s denominator -- `sum(done_g)` -- became the
blob's entire `.text` rather than what we have written.

The honest numbers, with the walk restricted to `build/src`:

```
  translated  [######............................]  17.1%    124247 bytes, 290 symbols
  tested      [#################################.]  97.7%    121351 bytes, 282 of 290 that can be
```

`translated` is unchanged from before d915b73 (17.1%, 124,247 bytes, 290
symbols).  The 98.0% never described any work; nothing is being reverted.

#### What 221 actually predicted, and what it looks like

221 said the `tested` figure should FALL when the denominator caught up.  It
does: 100.0% -> 97.7%.  The eight symbols that moved into the denominator are
exactly the file-local functions this tree has reconstructed and can now call
by name --

```
    v8_process       590    b103_process   582    v23_process   464
    AnalyseDialString 436   b103_create    406    v23_create    274
    v23_delete        72    b103_delete     72
```

-- 2,896 bytes that no test references a `ref_` alias of.  Task #62's tests are
what close that gap.

#### Whitelist, not a second blacklist entry

The fix walks `build/src` and nothing else, rather than adding
`dsplibs_glob.o` to the name test.  A blacklist has to be edited every time
the Makefile leaves a new intermediate in `build/`, and the failure mode when
it is not edited is this one: a number that is plausible, wrong, and committed.
Naming the one directory our own compiler writes to has no such edit.

The alias set is likewise read from `build/dsplibs_ref.o` rather than
re-derived from `symmap.py`'s exclusion rules, so the report cannot disagree
with the object the tests link; `make coverage` gained `$(REF)` as a
prerequisite, and `coverage.py` exits rather than guessing if it is absent.
`done_l` is still split into aliased and not, and the second half is empty
today: all ten of finding 221's multi-TU names are data symbols and
`our_symbols()` collects text only.  The bucket lists the ten anyway, derived
from the object rather than written down -- every file-local name that has no
`ref_` alias in `build/dsplibs_ref.o` -- because what cannot be tested
directly is worth naming whether or not anyone has reconstructed it.  The list
matches the one `symmap.py` prints on every run, which is the check that the
two are computing the same thing.

#### Two under-counts left standing, deliberately

`our_symbols()` collects `T` and not `t`, so a function we have reconstructed
*and also kept static* is invisible to the report.  Seven of finding 221's
fifteen are in that state -- `call_run`, `V34demodulate`, `v8_create`,
`v8_delete`, `call_create`, `call_delete`, `call_GetSRegister` -- reconstructed
here, static here, and counted as neither translated nor testable.  That is
why the eight above are eight and not fifteen.

All seven were exported in findings 223 and 224 and are counted now; the
under-count was real when this was written and is not a standing limitation of
the report.  What remains true is the shape of it: a function this tree keeps
static is invisible here, and if a future one is added it will be invisible
again.

And "what is left, by translation-unit span" iterated the globals alone, which
omitted the 47 file-local symbols nobody has reconstructed.  It now iterates
both.  Neither of these moves the headline; both were the same habit of
reaching for the globals because they were the easy half.

#### The lesson, which is 221's own

221's commit message is a story about a check that could not fail -- a
duplicate count taken on a deduplicated set.  It shipped, in the same commit,
a measurement that could not be right, because the thing it measured was
defined by exclusion and the exclusion was a literal filename.  A tool that
decides what counts as "ours" by listing what is not ours will be wrong the
first time something new appears, and it will be wrong quietly.  Compare
finding 198 and finding 199.

### 223. Fourteen of the fifteen, driven by name, with the objects compared

Finding 221 gave `ref_` aliases to the fifteen file-local functions this tree
had reconstructed.  Four new tests -- `t_b103direct`, `t_v23direct`,
`t_v8direct`, `t_calldirect` -- call fourteen of them by name.  Only
`V34demodulate` is left, for the reason at the bottom.

Calling them by name is the smaller half.  Three of the four datapumps were
already driven through their `dp_operations` table, and `b103_process` and
`v23_process` were reached by reading the function pointer back out of the
wrapper at `((struct dp_wrapper *)dp->dp_data)->process` -- indirect, and
quietly dependent on that layout being right, but not untested.  Two of the
fourteen genuinely had no route in at all: `call_GetSRegister`, which is not
in the operations table and which `call_create` plants in the supervisor's
configuration and nobody names again, and the direct entry to `v8_create` and
`v8_delete`.

**The half that matters is what is compared.**  The existing tests compare
shortlists: twelve fields of `struct b103_dp` (840 bytes), seventeen of
`struct v23_dp` (840), eight of `struct v8_dp` plus two of `struct v8`
(3,780), eighteen of `struct call_dp` (1,480).  In every one of them the two
bit buffers, the ring queues, the scratch areas and the whole embedded
supervisor went unread.  The four new tests compare the objects, after create
and after every block, with `diff_eq_obj`.

```
PASS b103_process: the whole object, per block   33607 checks
PASS v23 host: whole object                      67212 checks
PASS v8_process: the whole object, per block    159324 checks
PASS call: S56=2, tone                           13684 checks
```

Everything passes.  That is worth stating plainly: 3,780 bytes of V.8
handshake state, tracked block by block for 900 blocks through both of its
timeouts, agree with the blob byte for byte.

#### Normalising the pointers is where the mistakes are

A pointer holds an address, the two sides never agree on one, and the copy
that gets compared has to do something about that.  Three lessons, all of them
paid for:

**A pointer nobody has set holds the same fill on both sides.**  `struct v8`
has eight pointers that aim inside the object itself, so replacing each with
its byte offset from the base compares something real -- a pointer left aiming
at the wrong element of `seq[]` is exactly the defect that shape of field has,
and zeroing it would hide that.  But `seq_spare` is not written until a QCA1
message is accepted, so on a fresh object it holds `HARNESS_MALLOC_FILL`,
0xa5a5a5a5, and identically so on both sides.  Subtracting each side's own
base from that turns two EQUAL values into two different ones:

```
t_v8direct.c:213: handshake object [input 0]: struct v8+3152..+3153
    got a6 c4, reference b6 d3
    which field: tools/whichfield.py struct v8 3152
```

-- the low halves of `0xa5a5a5a5 - base_ours` and `0xa5a5a5a5 - base_ref`,
which is the distance between two allocations.  The conversion now applies
only to a value that really lands inside the object; anything else is compared
as it stands, which is what makes "neither side has set this yet" visible as
agreement rather than as a divergence.

**A function pointer can be checked against the right function, not merely for
being set.**  `struct callprog`'s `get_sreg` is `call_GetSRegister` on our side
and `ref_call_GetSRegister` on the blob's, so the two can never be equal.  The
weak normalisation -- reduce it to 0/1 like the heap pointers -- would pass if
a side planted the *other* side's callback, or any callback at all.  Each side
is instead required to have planted its own, which is a statement about the
wiring rather than about nullness, and it is the whole reason
`call_GetSRegister` is reachable at all.

**A motion guard has to scan, not compare the ends.**  Whole-object comparison
introduces a vacuity mode the existing anti-vacuity checks do not cover: two
objects agree when neither has moved.  Comparing the first block's state
against the last does not establish motion -- with a dial string too long to
prefix, `call_run`'s supervisor moves, finishes, and comes back to exactly
where it started, and that form of the guard called a live run vacuous.  Every
block is compared against the first.

#### Six functions lost their `static`

`v8_create`, `v8_delete`, `call_create`, `call_delete`, `call_run` and
`call_GetSRegister` were static here as they are in the object, so there was
nothing to call on OUR side either.  They are declared in the headers now.
Three of this tree's four datapumps already exported what the blob keeps local
-- `b103_process`, `v23_process` and `v8_process` among them -- so this is the
existing practice rather than a new one, nothing in `make phase` asserts
linkage parity, and a name that collided would fail at the link rather than
quietly.  `make phase` is green.

It moves the coverage denominator, which is expected and should not be chased:
`translated` went 290 -> 296 symbols as the six became visible to
`our_symbols()`, and `tested` 97.7% -> 99.7% as the tests landed.  Only
`AnalyseDialString` is left in the "alias exists and NOT tested" list.

#### `V34demodulate`: this section was wrong, and how

The first version of this finding said the fifteenth could not be driven
because "the four tests above all had a constructor to lean on... there is no
such constructor here", and recorded it as not attempted.

That was written without checking, and it is false.  `rxinit` is global on
both sides, `t_v34rx` already drives it, and it settles the AGC and the phase
accumulator -- it is exactly the constructor to lean on.  `t_v34rx` also
builds `struct v34_receiver` fixtures by hand in four places, and `harness.c`
cites one of them as the loop `diff_eq_obj` was written to replace.  The
evidence was in two files this session had already read.

Finding 224 has the test.  What is recorded here is the mistake: **"recorded
rather than attempted" is only honest when something was attempted.**  The
standing rule is to record the attempt and what stopped it; a premise nobody
checked is not an attempt, and writing one into the record is the failure mode
this tree keeps naming -- 221's claim that objcopy could not rename a local
symbol had sat in two docstrings for the same reason.

### 224. The two that had to have their calling convention read first

`AnalyseDialString` and `V34demodulate` are the last of finding 221's fifteen,
and they are the two the alias alone was not enough for.  Both are the shape
finding 51 warned about:

> A `t` symbol is not a testing inconvenience, it is a signal that the calling
> convention may not be the C one.  Check before writing the prototype, not
> after the comparison fails.

GCC picks its own convention for a static function whose callers it can all
see, and both of these have exactly one caller inside their own translation
unit.  So both were read out of the object before a prototype was written.

`AnalyseDialString`, at 0x7a9f0:

```
   7a9f7:  mov    %edx,%ebx          <- the string, in edx
   7aa04:  mov    %eax,%esi          <- the dialler, in eax
   7aae9:  mov    0x20(%esp),%ebx    <- `store`, on the stack
   7aab5:  ret                          caller cleans up
```

and from `IsDialStringInvalid`, the same statement from the other side:

```
   7af5b:  movl   $0x0,(%esp)        <- store
   7af62:  mov    %ebx,%eax          <- d
   7af64:  mov    %esi,%edx          <- s
   7af66:  call   7a9f0 <AnalyseDialString>
```

Two in registers and the third on the stack: `regparm(2)`.

`V34demodulate`, at 0x5af10, takes its one argument in `%eax` --
`mov %eax,%edi` before the frame exists, then `mov 0x4(%eax),%ecx` for the
queue's read cursor -- and `rxtiming` at 0x5b491 does `mov %esi,%eax` and
calls with nothing pushed.  `regparm(1)`.

Getting either wrong would not have failed to link.  It would have passed a
stack slot as a pointer, and the failure would have been a crash if we were
lucky.

#### What each test can say that the old route could not

`t_dialer` reaches `AnalyseDialString` through `IsDialStringInvalid`, which is
`grade <= DIALER_INVALID` -- so it sees a boolean where the function returns
one of four grades, and it never passes `store`.  Its own comment says the
four grades are "asserted against our own implementation alone and marked as
such: claims about what the disassembly says, not about what the blob does".
`t_dialstring` calls it directly, so they are claims about the blob now: 34
strings at 16 flag combinations at both values of `store`, then every byte
value 1..255 in the dispatched position in both modes, with the whole
`struct dialer` compared each time.  All four grades come back, which is the
guard against a sweep that compared one answer with itself.

`t_v34rx` says of `V34demodulate` that it "is a local symbol and can only be
reached through the interpolator, so every AGC defect it had presented as a
loop-shape failure".  `t_v34demod` drives it directly over 180 fixtures --
the freeze flag, amplitudes from silence to clipping across the RMS floor at
31, gains from unity to saturation, and four carrier steps -- 24 calls each,
with the whole `struct v34_receiver` compared after every one.  4,320 object
comparisons and no divergence.

Both sides are pointed at ONE carrier table, declared in the test.  `rxinit`
does not install one -- that is chosen by symbol rate elsewhere -- and a table
per side would be two arithmetics on two inputs.  Which coefficients belong to
which rate is a different test's subject.

#### The receiver's three pointers are compared, not skipped

`t_v34rx` skips `rx_samples` in four places, because "the two installed
pointers differ by construction".  They do, but only in their base: `rxinit`
sets it to `(char *)rx + 0x10c` and `V34demodulate` advances it by one short
per call.  It, and the receive queue's `rd` and `wr`, are replaced by their
byte offset from the receiver, which the two sides must agree on exactly.  A
cursor left one short behind is the defect that shape of field has, and a skip
cannot see it.  Same treatment as `struct v8`'s eight in finding 223, and with
the same caveat: only a value that really lands inside the object is
converted.

#### Where this leaves the count

```
  translated  [######............................]  17.5%  127543 bytes, 297 symbols
  tested      [##################################] 100.0%  127543 bytes, 297 of 297
```

Every symbol this tree has reconstructed that can be driven by name is driven
by name.  100.0% is not a claim that the reconstruction is finished -- 17.5%
is the honest half of that pair, and `translated` is what has to move next.
It says the two numbers now measure what they say they measure, which is what
findings 221 and 222 were about.

### 225. Nothing demangles, and from C++ that is a link error that reads wrong

`symmap.py` prepends the prefix to the raw symbol string.  The blob's
`_ZN12VPcmFloModem11enterPhase3Ev` becomes
`ref__ZN12VPcmFloModem11enterPhase3Ev` -- note the doubled underscore -- and
886 of the aliases in `build/dsplibs_ref.o` are this shape.  `tuattrib.py`
demangles through `c++filt` for translation-unit attribution, but nothing on
the rename path does, and nothing needs to: a mangled name is already a valid
C identifier.

From C it works, verified:

```
extern int ref__ZN12VPcmFloModem11enterPhase3Ev(void *self);
    -> links and resolves
```

**From C++ it does not, and the error misdirects.**  A plain declaration is
mangled a second time, as an ordinary function whose *name* is that string:

```
extern int ref__ZN12VPcmFloModem11enterPhase3Ev(void *);
    -> U _Z36ref__ZN12VPcmFloModem11enterPhase3EvPv

extern "C" int ref__ZN20V90Phase3Demodulator5resetEv(void *);
    -> U ref__ZN20V90Phase3Demodulator5resetEv
```

The undefined symbol in the first case *contains* the name that was wanted, so
the link error reads as "the blob does not export this" rather than "my
declaration was mangled".  That is the misreading worth guarding, and #60 is
fifty opportunities to make it: `VPcmV34Main.cpp` is the C++ half, its
reconstruction is C++, and every one of its `ref_` aliases is mangled.

`harness.h` now says so at the top of its `extern "C"` block, which is where
such declarations belong.

#### The reconstruction writes real C++, and the mangling is a free type oracle

The `ref_` prefix is a harness detail and does not reach `src/`.  What we write
is ordinary C++ with the original's own class and method names, and the
compiler reproduces the blob's symbol exactly.  `src/dsp/FloatIIR.cpp` is the
working precedent:

```
blob   W _ZN10GenericIIRIfdE5resetEv        (.gnu.linkonce.t.*)
ours   W _ZN10GenericIIRIfdE5resetEv
```

five aliases, driven by `test/unit/t_genericiir.cpp` -- a C++ test, so the
pattern for #60 is established rather than new.  (The blob also has a separate
concrete `_ZN8FloatIIR...` class, four symbols, not yet reconstructed;
grepping the object for "FloatIIR" finds that one and it is easy to mistake
for a mismatch.)

**This means the mangled names are a type oracle, and the C half has nothing
like it.**  For a C function the parameter types come out of the disassembly
-- finding 51's regparm work, argument widths read from how each is used.  For
C++ `c++filt` simply says:

```
$ c++filt _ZN20V90Phase3Demodulator5resetE7PcmTypeh22Phase3DemodulatorStatejP5V90JdP5V92JdP19tagV90DILdescriptorssfj
V90Phase3Demodulator::reset(PcmType, unsigned char, Phase3DemodulatorState,
    unsigned int, V90Jd*, V92Jd*, tagV90DILdescriptor*, short, short, float,
    unsigned int)
```

Eleven parameters, in order, with the enum and struct names the author used.
For #60's fifty symbols that is the whole signature problem solved before any
disassembly is read.

The obligation runs the other way too: **reproducing the mangling means
reproducing the declaration exactly** -- class name, method name and every
parameter type.  A tidier signature, a generalised template, an `int` where
the original had `unsigned`, and the compiler emits a different symbol which
links against nothing and is silently not the function.  The mangled name is
the specification, not a decoration.

#### Not fixed, and deliberately

The alias could be `ref_` spliced *inside* the mangling, so that
`_ZN12VPcmFloModem11enterPhase3Ev` became
`_ZN12VPcmFloModem14ref_enterPhase3Ev` and demangled to something a C++ test
could declare naturally.  That means teaching `symmap.py` to parse Itanium
mangling, for a cosmetic gain over one `extern "C"` block, and a mangling
parser that is subtly wrong renames a symbol to something that still links --
the worst failure available here.  Left alone on purpose.

### 226. The C++ half's class structure is already written down, in the mangling

886 of the blob's symbols are Itanium-mangled, across **73 classes and 55 free
functions** -- essentially all of `VPcmV34Main.cpp` and the float DSP beneath
it.  Every one encodes the class, the member, and each parameter type in
order, with the author's own enum and struct names.  `tools/cppstruct.py`
reads it out:

```
$ tools/cppstruct.py FloatFIR
class FloatFIR {
        ? process(float const*, float*, unsigned int);     // 287 bytes, NOT WRITTEN
        ? process(float);                                  // 182 bytes, NOT WRITTEN
        FloatFIR(unsigned int, float*, unsigned int);      // 105 bytes, C1,C2
        ? reset();                                         //  61 bytes
        ? setCoefficients(float*, unsigned int);           //  58 bytes
        ~FloatFIR();                                       //  30 bytes, D1,D2
};
```

For the C half every one of those argument lists costs a disassembly read.
Here it is free, and it is the difference between starting #60 from a
skeleton and starting it from a guess.

**`?` is deliberate.** Itanium mangling omits return types for ordinary
functions, so the tool emits a token that does not compile rather than `void`,
which would let a skeleton be pasted in and built with every return silently
wrong.  Also absent: member variables, base classes, access specifiers, and
any member the compiler inlined everywhere.  Bound object sizes the way
finding 215 did, from the largest `this`-relative displacement.

#### Three things it found that were not known

**`FloatIIR` and `FloatFIR` are unwritten, and are not what we thought.** The
blob has a concrete `FloatIIR` (5 members, 575 bytes) *and* a
`GenericIIR<float,double>` template instantiation (1,269 bytes, weak).
`src/dsp/FloatIIR.cpp` reconstructs the *instantiation* -- correctly, matching
the mangling, tested by `t_genericiir.cpp` -- so the file name names a class
it does not implement, and grepping the object for "FloatIIR" finds the other
one and reads as a mismatch.  `FloatFIR` (6 members, 723 bytes) is untouched
and one of its members, `setCoefficients`, is inside #60's batch 2.

**`V90PreFilter` is mostly coefficients.** Its code is about 2 KB, but it
carries **nine static tables totalling ~21 KB** -- `preFilterCoefType1..3`,
`refLoopsType1..7`, `dataBase`.  #60's brief called it a 1,280-byte object on
the strength of its `this`-relative displacements, which measured the object
and not the class.  Static members are `.data`, so they are outside #60's
16,003 bytes of text and were invisible to every count made so far.

**Data members were being read as functions.** A demangled name with no
parenthesis is data, and the first version of this tool sent those to the
free-function list, where `V90PreFilter::preFilterCoefType3` appeared as the
second largest "function" in the object at 4,960 bytes.

#### And two bugs in the tool worth the same note as finding 221's

The constructor/destructor tag sits directly after the class *name*
(`_ZN18V90Phase3ModulatorC1EP13V90Parametersj`), so it follows a letter.  Two
versions anchored on the end of the symbol and on a preceding digit; both
matched nothing and printed `None` for every constructor in the object,
silently, because a missing annotation looks like an absent feature.
### 227. The #59 warm-up trio, and the byte past `getbit` settled

*(Numbered 226 on branch `v90cpp` while it was being written; renumbered to
227 on merge, because master had meanwhile taken 226 for `cppstruct.py`.  If
you find a reference to "finding 226" meaning the warm-up trio, it means
this.  Master's 225 and this branch's 225 were the same discovery made twice
independently -- master's wording is the one that survived, and the fact is
unchanged.)*

`getbit` (433 bytes, file-local, **recursive**), `ApplyBulkDelay` (467,
file-local) and `_Z14getMPrecvdBitsP12tagV34Object` (895, file-local and
C++-mangled) all landed, all three by direct differential test against the
blob rather than through a caller.  Finding 221's two-pass `objcopy` is what
made that possible; finding 117 and finding 173 had both concluded the
opposite, correctly, from an `objcopy` invocation that could not promote a
local.

#### What the three do

`getbit` is the object's bit reader for a V.34 handshake message.  An
accumulator at +0x24 and a count of unread bits at +0x28; every call returns
the top unread bit and drops the count.  A refill takes either a whole word
or, when fewer than `wordbits` remain, a PART word -- and the part-word arm
shifts by what is left and ORs the WHOLE word in anyway, without advancing
the index.  `crc_on` at +0x16 appends CRC-16-CCITT (0x1021, MSB-first,
unreflected, the same one `v8_crc` computes) as sixteen more bits once the
message runs out, and `repeat` at +0x20 restarts the reader and **calls
`getbit` again** for the first bit of the repeat.  Exhausted with neither, it
returns -1.

**And one arm that reads as arbitrary until you drive it.** If the remainder
`nbits - pos` is EXACTLY -16, the reader loads 0xf and four bits instead of
doing any of the above, so the next four calls return 1,1,1,1.  Nothing else
in the function tests a particular negative remainder.  The test shows where
it comes from: the CRC costs sixteen bits of `pos` that `nbits` does not
cover, so the refill *after the CRC* sees precisely -16 every time.  It is
the filler between a message and its repeat, not a special case.

`ApplyBulkDelay` sizes the far echo canceller's ring from the measured
round-trip delay: a delay at or below zero becomes 144, a delay at or past
`bulk_len` becomes zero, the ring at +0x35b8 is cleared to the result and
`bulk_head`/`bulk_tail` are reset.  Then, with neither PCM receiver running
and the far canceller already armed, a delay under 30 disarms it and pulls
the DMA delay at +0x25c back by `delay + 15` capped at 30.

`getMPrecvdBits` copies the V.90 MP sequence out of the session -- six flag
bytes and seven shorts at `p3548 + 0x1744` -- into the INFO record at
+0xaa0c, copies the four-bit rate nibble down over bits 2..5, and then
rebuilds the capability word at +0xaa3c around the largest upstream rate the
configuration allows, reversed into bits 6..9.

#### FINDING 173's OPEN QUESTION, ANSWERED: it is alignment padding

173 recorded that `getbit` ends at 0x5eaf0 + 0x1b1 = 0x5eca1 and that one
byte past the symbol there is `eb 0d  jmp 5ecb0 <setfinalrate>` -- either
padding that happens to decode as a jump landing exactly on the next
function, or a tail call the symbol size excludes.  **It is padding.**  Three
things settle it and none of them is the decode:

  - `getbit`'s last instruction, at 0x5ec9c, is `e9 03 ff ff ff` -- an
    UNCONDITIONAL jump, ending exactly at 0x5eca1.  Nothing falls through
    into the byte in question.
  - Nothing branches to it.  A disassembly of the whole 0x9c000-byte `.text`
    contains the string `5eca1` exactly once, and that once is the
    instruction itself.
  - **51 inter-function gaps in this object are exactly 15 bytes, and every
    one of the 51 is `eb 0d` followed by thirteen `nop`, with the jump
    landing exactly on the next symbol.** 0x5eca1 is one of the 51.  The
    other fills gas uses here are plain `nop` runs (835 gaps) and multi-byte
    `lea` no-ops (637); the short jump is what it emits for a 15-byte gap.

So the symbol size is right and excludes nothing, and `setfinalrate` is
16-byte aligned.  173's alternative -- a tail call -- is refuted by the
unconditional jump immediately before it: a tail call would have to be
reachable.

#### The V.34 object, for the V.90/V.92 batch

  - **+0xaa3c is a `getbit` message, and +0xaa6c is the pointer to it.**
    `getMPrecvdBits` writes `obj + 0xaa3c` into `obj + 0xaa6c`, and
    `v34handshak` loads +0xaa6c thirty-six times and hands what it finds to
    `getbit`.  So the bit reader's layout maps onto the object as
    `word[10]` at +0xaa3c..+0xaa4f, `crc` at +0xaa50, and the rest of the
    state through +0xaa6b -- which is `unmapped_aa40` in `v34fsk.h` today.
    `struct v34_bitsource` is declared standalone rather than embedded
    there, because +0xaa3c and +0xaa3e are already named `info_caps` and
    `caps_flags` from the other direction and neither reading is more
    correct than the other.  **The array length of ten is adjacency, not a
    bound**: nothing in `getbit` checks the index.
  - `getMPrecvdBits` and `ApplyBulkDelay` both reach `v90_receiver` and
    `k56flex_receiver` through `obj + 4` with a 0x248/0x24c displacement,
    which is the third and fourth file to do so.  `v34fsk.h` already says
    this is an addressing artifact and not a sub-object; it now has four
    witnesses.
  - The session object gains three fields: `+0x1744` is an MP block of six
    bytes and seven shorts, and the PCM receiver at `+0x610c` has an int at
    `+0x4f8` (the "sensitive ISP" flag) and a rate index at `+0x4fc` that is
    multiplied by 2400.  The configuration at `pac3c + 0x3c` is an upstream
    rate in bits per second.
  - Rate index from bits per second is `(bps * 7) >> 14`, not a divide by
    2400: the constant is 1/2340.6, and 33600 still lands on 14 because the
    input is capped at 0x833f = 33599 first.

#### Two things the tests had to be built around

**A duplicated call, which is the object's.** `getMPrecvdBits` tests bit 0 of
`info_rates` and calls `txrxdmainit` inside the V.90 branch, then tests the
same bit again after it and calls `txrxdmainit` a second time.  With a V.90
receiver running and the bit set it runs twice over the same six shorts.  It
is idempotent, so this is wasted work rather than a defect; it is recorded
because a reconstruction that "tidied" it would still pass every byte
comparison.

**An unsigned bound that is the wrong way round.** `ApplyBulkDelay` compares
the delay against `bulk_len` with `jb`, not `jl`.  A NEGATIVE `bulk_len` is
huge unsigned, so it accepts every delay and the clear then runs off the end
of the ring.  Nothing reconstructed writes `bulk_len`, so no call site is
known to reach it, and the tests keep it positive deliberately: the overrun
would be identical on both sides and would prove nothing while corrupting
the object under comparison.

#### How they were tested

`getbit` and `ApplyBulkDelay` went into `test/unit/t_v34hshak.c`, the test
for their own translation unit; `getMPrecvdBits` needed a new C++ test.

`getbit` is the first recursive function this tree has driven against the
blob, and a sweep that only ever took the base case would not be a test of
it.  Fifteen seeds are driven past exhaustion, 2,358 calls, with the whole
0x30-byte reader compared after every single one -- and **four anti-vacuity
flags assert that the recursive arm ran, that the -1 return happened, that
the CRC tail ran and that the four-bit filler ran.**  The struct is seeded
whole rather than filled with `HARNESS_MALLOC_FILL`, because `idx` is an
array subscript with nothing bounding it and 0xa5a5 is a wild read.

`ApplyBulkDelay` gets 1,008 cases sweeping both sides of every boundary,
whole-object each time, plus 448 with both debug levels at 2 and the
transcripts diffed -- because its TWO rejections print the SAME format
string, and no byte comparison can tell a reconstruction that printed the
wrong one.

`getMPrecvdBits` gets 784 whole-object cases, 720 transcript cases and five
that read the CLAMP back out of the object.  That last block exists because
the obvious anti-vacuity flag for a branch is the branch condition restated,
and a flag satisfied by the same misreading that produced it proves nothing:
both sides are fed the same inputs and the clamp only decides which of two
rates reaches `bitreverse`.  So the four bits are taken back out of +0xaa3c,
un-reversed by hand rather than by calling `bitreverse` again, and compared
against the index the SMALLER of the configured rate and the PCM cap should
give -- with one case that shuts the sensitive arm off and must therefore
show the LARGER, so "the clamp happened" and "the clamp is unconditional"
are different results.  Its
+0xaa6c is a SELF-pointer, so it is checked against *this side's own*
`m + 0xaa3c` rather than against the other side's value or against being
non-null -- finding 224's rule.  Which of the two `edprintf` arms ran is
**not legible in the transcript**: `edprintf` prints its message encoded, and
the plain-text switch that would undo that is ours alone, so setting it would
make the two sides differ for a reason unrelated to the modem.  The arm is
named from the inputs instead, and what the transcript is asked is that the
two arms do not print the same thing.

#### AND TWO CHECKS THAT COULD NOT SEE C++ AT ALL

`tools/debugaudit.py` -- the `strings` gate that catches an invented format
string -- and `tools/debugcov.py` both globbed `src/**/*.c`, and `debugcov`
built only `$(TESTS)`.  `FloatIIR.cpp` prints nothing and has no test outside
`$(CXXTESTS)`, so nothing had ever exposed either gap.  `getMPrecvdBits`
brought seven format strings in a `.cpp`: they would have gone unaudited, and
their sites would have counted as dead because the only test that drives them
is a `CXXTESTS` member the instrumented tree never built.

Both tools now take `.cpp`, and `debugcov` builds both lists.  517 strings
checked where it was 501, still 0 invented; 327 debug sites where it was 320,
still 30 dead.  This is finding 134's argument arriving from a new direction:
a check nobody can see failing is a check that reports clean because it
cannot fail.  **The V.90/V.92 core is entirely C++, so both gaps would have
widened with every task in #60 had this one not carried a diagnostic.**

### 228. "No virtuals" was asserted in a build comment, repeated into a brief, and wrong

The Makefile said the C++ was built "-fno-exceptions -fno-rtti with no
virtuals and no new/delete, exactly as the original was built".  Three of
those four are right.  The object has **four vtables**:

```
$ nm --print-size ../slmodemd/dsplibs.o | grep ' V '
24 V  vtable for Resampler                28 V  vtable for V90Resampler
28 V  vtable for ResamplerTiming          24 V  vtable for ResamplerTimingOffset
```

**The rest of the clause stands, and the absence of typeinfo is what proves
it.**  Zero `_ZTI` and zero `_ZTS` alongside four vtables is precisely what
`-fno-rtti` *with* virtual functions emits — had RTTI been on, each vtable
would carry a typeinfo pointer to a real `_ZTI` object.  Also zero `__cxa_*`,
`_Unwind_*`, `_Znw*`, `_Zdl*` and `_ZdaPv`.  So exceptions, RTTI and
new/delete are all still correctly described, the link still needs no
libstdc++, and a correction reading "the -fno-rtti claim was wrong too" would
replace one error with a worse one.

**The dispatch is live, not vestigial.**  `Resampler::timingCorrection(float)`
is **one byte** — a bare `ret`, an empty base implementation — and it is the
only `W` (weak) symbol of the group, while two derived classes replace it:

```
    1 W  Resampler::timingCorrection(float)
  191 T  ResamplerTiming::timingCorrection(float)
   14 T  ResamplerTimingOffset::timingCorrection(float)
 1039 T  Resampler::resample(float const*, unsigned int, float*, unsigned int&)
  170 T  V90Resampler::resample(float const*, unsigned int, float*, unsigned int&)
```

#### Why it matters here, which is layout and not the build

A virtual class carries a vptr at offset 0, so every member of those four sits
four bytes further along than a non-virtual reading of the disassembly would
put it.  That collides with the object-sizing method finding 215 established
and #60 uses — bound the object by the largest `this`-relative displacement.
The *bound* stays right; the *field map* derived from those displacements is
shifted.  A struct correct in size and wrong by four in every offset passes a
size check and fails everything downstream of it.

`ResamplerTimingOffset::setTimingOffset(float)` is one of #60's fifty, so this
is not hypothetical.  It happens to be safe — the whole function is

```
    35510: mov   0x4(%esp),%eax        ; this: first stack arg, cdecl (finding 215)
    35514: flds  0x2c(%eax)
    35517: fmuls 0x8(%esp)             ; the float parameter
    3551b: fmuls 0x1f4  <R_386_32 .rodata.cst4>
    35521: fstps 0x48(%eax)
    35524: ret
```

which touches `+0x2c` and `+0x48` and never offset 0, so both sides leave the
vptr slot alone and it compares equal whatever we model there.  **Declaring
real `virtual` members is the branch to avoid**, and it is rejected rather
than merely unchosen: GCC would then emit a vtable, which needs every virtual
method defined or a key function present, reopening the link closure of
finding 225's kind for a twenty-one-byte float setter.

#### The cheap check, which already existed

`tools/cppstruct.py <class>` answers "is this polymorphic" without going near
a vtable symbol.  A destructor listed with a `D0` variant is a *deleting*
destructor, which GCC emits only for a virtual one:

```
$ tools/cppstruct.py Resampler
        ~Resampler();          // 85 bytes, D0,D1,D2, NOT WRITTEN
```

#### How it survived

The claim was load-bearing for nothing.  The build works either way: our own
C++ genuinely has no virtuals, so `-nostdinc++` and linking with `$(CC)`
succeed whatever the original did.  Nothing could fail until someone had to
lay out a polymorphic class, which is #60 and no earlier task.  It then got
repeated verbatim into a task brief as settled fact, which is the mechanism
worth noticing: an unchecked claim in a build comment is quoted onward as
though the build had checked it.

The Makefile comment is corrected in place.  `CLAUDE.md` was checked and never
carried the claim — the second copy was in the #60 brief, which is outside
the repository and cannot be corrected from here.
