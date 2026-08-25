# A patched `dsplibs.o` exists in the wild

`cryan209/D-Modem` (branch `pjsip2.15`) is a fork of the `strozfriedberg/D-Modem`
this reconstruction targets. Its `slmodemd/` ships **three** copies of the blob:

| file | size | md5 | what it is |
|---|--:|---|---|
| `dsplibs.o.bak` | 1,233,728 | `1fd60268a1dcf5392f5520791f7a7059` | **byte-identical to our target** |
| `dsplibs.o.mod` | 1,233,728 | `04fabcb2ba7f293165cd6ac6f4977f50` | `.bak` with **32 bytes of `.text` hand-patched** |
| `dsplibs.o` | 1,232,028 | `56745e1162d138b9a4c7d2025c9b69f2` | the one their Makefile links |

Everything below is measured, not inferred from the fork's prose. Findings
F1140-1146 carry the evidence.

## None of the three is a different build

All three carry the same `.comment` — `GCC: (GNU) 3.4.2 (Gentoo Linux
3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)`, repeated 279 times, which is finding
F606's fingerprint exactly. All three have `.text` at file offset 0x40, size
0x0b1cf0, and the same 152 sections with the same names and the same sizes.
The 283 `FILE` symbols are identical, and the relocation count is 18,317 in
every case.

**There is no second build of slmodem here.** Anything this tree learns from
our blob applies to all three.

## The shipped `dsplibs.o` is the SAME CODE with an override hook

Its `.text` is byte-identical to ours (`md5 d25e747af8636e4ee2250d33737bbc17`;
`cmp` reports zero differences). It is 1,700 bytes smaller for two unrelated
reasons, neither of them code:

- It was rewritten by a **modern binutils**. `.shstrtab` moved from section
  index 148 to 150, the `REL` sections gained `SHF_INFO_LINK`, and 56
  `SECTION` symbols for `.rel.*`, `.symtab`, `.strtab` and `.shstrtab` were
  dropped. Pure metadata; `.symtab` shrinks 0xc0a0 -> 0xbd20 and `.strtab`
  0xfba5 -> 0xf884.
- **`VPCMXF_Create` was weakened and aliased.** It is `GLOBAL` in ours and
  `WEAK` in theirs, at the same address (0x00fcf0) and the same size (495),
  and a new `GLOBAL` symbol `__blob_VPCMXF_Create` was added at that address.

That second change is the classic link-time interposition trick: a strong
definition of `VPCMXF_Create` in the fork's own source now wins over the
blob's, and the original stays callable as `__blob_VPCMXF_Create`. **The
shipped blob's code is unmodified; only its linkage was changed.**

`VPCMXF_Create` is exactly the function whose first argument decides which
side of a V.90 call the object is (finding F701), so that is the knob the fork
made reachable from C without touching the binary. Their own
`slmodemd/dp_vpcm_shim.c` records the recipe in a comment —
`objcopy --weaken-symbol=VPCMXF_Create --add-symbol
__blob_VPCMXF_Create=.text:0xfcf0,global,function` — though no Makefile rule
runs it, so the blob is a committed artefact and the patch is not reproducible
from their repository.

**The override is off by default.** Their strong `VPCMXF_Create` forwards the
argument unchanged unless `SLMODEMD_VPCM_DIGITAL_SIDE` is set in the
environment, in which case it forces the value to 1 and logs "overriding side
%d -> 1 (Digital)". So a default build of the fork is the analogue client,
exactly as ours is, and the digital side is a run-time opt-in that nothing in
their repository shows anyone having exercised successfully.

## `.mod` is a hand-edited byte patch that switches sides

`.mod` differs from `.bak` in **32 bytes across 12 runs, all inside `.text`,
and nothing else in the file changes**. Every edit is length-preserving,
padded with `nop` where the replacement is shorter — the signature of a hex
editor, not a rebuild.

Site by site (`.text` offsets; add 0x40 for file offsets):

| site | before | after | effect |
|---|---|---|---|
| `vpcm_create+0xf5` | `movl $0x0,(%esp)` | `movl $0x1,(%esp)` | **`VPCMXF_Create` arg0 -> non-NULL -> side becomes DIGITAL** |
| `vpcm_create+0x180` | `xor %eax,%eax` | `mov $0x1,%al` | forces `_tagModemParameters+0x002` bit 4 (**V.92**) ON when the requested datapump is not V.92 |
| `v8_create+0xc7` | `jne 0x36c0` | `jmp 0x36c0` | drops the `caller` test from `params+0x000` bit 3 (**V.90/V.92 offered**) |
| `v8_create+0xed` | `setne %dl` | `mov $0x1,%dl` | with the next site, forces `params+0x002` bit 4 (**V.92**) ON unconditionally |
| `v8_create+0xfb` | `and %cl,%dl` | `nop nop` | removes the `dp_id == V92` conjunct |
| `VPcmFloModem` ctor `+0x60` (both `C1` and `C2`) | `sete %dl` / `sete %al` | `mov $1,...` | forces `V92ModemSide` = 1 (analogue) for the `V92Modem` sub-object |
| `VPcmFloModem` ctor `C1+0x25c` | `mov %esi,0x6120(%ebx)` | `mov %edi,...` | stores 0 instead of the side, undoing the previous site's effect on that field |
| `VPcmFloModem::externalReset+0x111` | `jne 0xd832` | `nop nop` | **never calls `V90Demodulator::reInit()`** |
| `rebuildJMSequence+0x136` | `xor %edx,%edx` | `mov $0x1,%dl` | **INEFFECTIVE** — `setne %dl` two instructions later overwrites it |
| `rebuildJMSequence+0x6a1` | `je 0x75f82` | six `nop` | removes the `params+0x002` bit 2 guard — offers a menu item even when the far end did not ask for it |
| `rebuildJMSequence+0x7e8` | `sete %cl` | `xor %cl,%cl` | removes the fallback that emits the bare JM word `0x00a9` |

Note that `C1` and `C2` are GCC's two clones of the *same* `VPcmFloModem`
constructor, not two members called C1 and C2 — the patch hits the same source
line in both, which is a useful consistency check on the reading.

### The headline: it is the digital side

`vpcm_create` is the only caller of `VPCMXF_Create` in 1.2 MB, and
`VPCMXF_Create` computes `side = (arg0 == NULL)` and does nothing else with
that argument — it is never dereferenced (the register is reloaded from
`sysdep_malloc`'s return before any memory access), so passing the literal `1`
is safe. The blob names the two values itself, in its own debug print:

```
VPCMXF_Create: side is %s, maxDataBuffer - %d
                       ^-- "Digital" when arg0 != NULL, "Analog" when NULL
```

and it scales `maxDataBuffer` by **8.0** on the digital arm against **9.6** on
the analogue arm — 8000 Hz against 9600 Hz, the network PCM rate against the
client's internal rate.

So `.mod` turns the object into the **digital (server) side of a V.90 call**:
the modulator branch findings F701 and F702 costed at 26 KB. Three further sites
(`ctor+0x25c`, `externalReset+0x111`, and the `V92ModemSide` compensation at
`ctor+0x60`) are consistent housekeeping for that switch.

**What it does NOT do is make the branch work.** It selects code the vendor
never shipped as reachable; nothing here says that code is finished. Finding
F702's caution stands unchanged, and `.mod` is not evidence that anyone ever
got a call up in this mode — it is an inert file beside the one that ships.

**The author says the same thing.** `.mod` entered their history as *"dodgy
patches to dsplibs.o to see if it can be enticed to be PCM side"* and left it
one commit later as *"revert dsplibs.o"*. That is the reading above, arrived
at here from the bytes alone, in the patcher's own words — including the
verdict.

## Does any of it explain "up to full 56k"?

Not through `.mod`. Not one of the 32 bytes touches V.90 rate selection, the
PCM constellation, or the digital-impairment learning that decides a 56k rate.
What they touch is **which capabilities V.8 advertises** (`params+0x000` bit 3,
`params+0x002` bit 4, and two conditions inside the JM builder) and **which
side the object is**. Those are negotiation-time flags, not rate-time ones.
`vpcm_create` still clamps the host's maximum to 0xdac0 = 56,000 and still
writes the literals 4800 and 33600, unpatched.

**And the claim is not even the same author's work.** The README bullet about
"up to full 56k" was added in February 2025 by Michael Gernoth; every
blob-related commit is from March 2026 and by someone else. The 2025 change it
describes is host-side: a resampler bridging 8 kHz RTP to the 9600 Hz the
object actually wants (`RcFixed_Resample` in `slmodemd/modem_main.c`, absent
upstream), plus a change to what `MDMCTL_IODELAY` reports. Neither touches the
object's code, and the rate this tree pinned down in findings F17 and F23 is
exactly the one that resampler exists to serve.

Their HEAD is also no longer the configuration that bullet was measured on:
`MDMCTL_IODELAY` was later hardcoded to 48 where the tested tree returned
`MODEM_FRAMESIZE` (192), and `MDMPRM_CODECTYPE` was changed to an "unknown
codec" value. Treat the README as a claim about a 2025 tree, not this one.

## Practical consequences for this tree

- **Our target is `.bak`, which is also the shipped `.text`.** Nothing about
  the fork changes what we are reconstructing.
- **`__blob_VPCMXF_Create` is a name to recognise, not to reproduce.** A
  reconstruction that emits `VPCMXF_Create` as a normal `GLOBAL` is correct;
  the weak alias is a downstream packaging decision.
- If the digital side is ever built here, `.mod` is a worked example of the
  minimum edit that reaches it: one immediate byte in `vpcm_create`. The
  fork's shim is the same switch done properly, from C.
- **Their `doc/` tree is not evidence.** The fork carries a large corpus of
  automatically generated per-function disassembly and pseudo-C. It may be
  useful for orientation, but under this tree's rules it has exactly the
  standing `tools/decompile.sh` output has: scaffolding, never evidence, and
  nothing from it may be written into a finding.
