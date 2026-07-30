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
