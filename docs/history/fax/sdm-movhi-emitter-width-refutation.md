# SDM MOVZWL does not recover an explicit wide RTL read

The proposed combine-narrowing discriminator is refuted by the saved controls.
Plain-postincrement UID56 is already `SET(REG:HI, MEM:HI)` in `01.rtl`; it remains
HI in `19.life` and `20.combine`. There is no observed combine transition from
an SI zero-extend to this HI read. Compound-postincrement UID57 is likewise an
HI read from initial RTL. No narrowing function/rule can be assigned to a
transition these controls do not contain.

More decisively, the compound control's reload-added UID118 retains plain
`SET(REG:HI DX, MEM:HI DI)` through final RTL and nevertheless emits
`movzwl (%edi), %edx`. Its saved `SDM.s` lines 744–747 annotate exactly this
pattern as code 40, `*movhi_1/3`. The genuine SI zero-extend UID38 emits the
same load mnemonic with pattern `*zero_extendhisi2_movzwl` at lines 701–703.
Consequently a blob MOVZWL is compatible with both patterns. Its mnemonic alone
does not establish the original source promotion or explicit SI RTL role.

## Installed compiler emitter witness

The installed, hash-pinned Gentoo cc1 was observed at `output_40`, UID118,
without inferior calls or RTL changes:

| Observed property | Value |
|---|---|
| Pattern | SET, destination HI, source HI |
| `which_alternative` | 2, the third alternative |
| `ix86_tune` | `PROCESSOR_PENTIUMPRO` |
| MOVX / PARTIAL_REG_STALL / HIMODE_MATH tune bits | 8 / 8 / 0 |
| `get_attr_type` result | `TYPE_IMOVX` |
| Returned emitter template | `movz{wl|x}\t{%1, %k0|%k0, %1}` |

The bounded witness denominator is **one output_40 UID118 visit**. The complete
emitter-debugged object, raw replay object and previously audited raw object
are **3/3 identical**, SHA-256
`e0f33cadc15147ce8e0e71eb54c07aae76736a7481126dc3a40bdd6d4671599e`.
The prior three-control audit establishes that previous raw object also equals
the saved container object. Runtime and toolchain provenance distinctions from
[the dynamic postreload trace](sdm-postreload-dynamic-trace.md) still apply:
unchanged image cc1 under host gdb/runtime, image-selected assembler.

## Backend rule

Official GCC 3.4.2 `i386.md:1295–1344` defines `*movhi_1`. For the memory-load
alternative, an aligned operand can select ordinary IMOV first; otherwise a
MOVX-enabled tune selects IMOVX, and the output function returns MOVZWL with
the SI register spelling `%k0`. The accompanying comment says this avoids
partial-word stalls. This is backend instruction selection for a HI move,
not evidence of an SI value surviving in RTL. The tuning masks are defined in
`i386.c:476,485,498` and selected through `i386.h:258` and its MOVX macro.
The actual Gentoo dynamic result corroborates this stock-source explanation.

Pinned stock source files are preserved in the integration worktree's
`build/gcc-x87-mechanism-source` directory:

- `i386.md`: `2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab`
- `i386.c`: `bb46f666e8686bf902ae1f9f32fcbfb298259915c1368e669326c89adce34cdc`
- `i386.h`: `a88606d1f36e9cc667805bd22b19333608f6aea55beee932d3767079284e95ec`

These are official stock source pins, not a claim to possess the full Gentoo
patched generated files. DWARF and the actual emitter observation come from
the installed Gentoo binary.

## Reproduction

First create the three-control dynamic replay using its existing driver. Then:

```sh
python3 tools/gentoo_cc1_sdm_movhi_reproduce.py \
  --replay build/gentoo-cc1-trace-full --output build/sdm-movhi-replay
```

The wrapper reuses the complete saved raw/debugger commands and unchanged
preprocessed full TU, changes only the GDB observation helper and apparatus
directory, checks exactly one witness, and assembles raw/traced outputs with
the same image-selected assembler. It records executed commands, logs, source
paths and hashes in its output directory. The helper refuses a cc1 hash other
than `80a79e520ca77fb1efa3bbd5ac4d927d075d6c1fdc6dd908fd83b437cc47febc`.
UID118 is selected within this bounded full-TU control and corroborated against
annotated assembly; no universal UID or ABI-function mapping is claimed.

Initial exploratory emitter attempts with incorrect access to an RTL field
were rejected as debugger errors and remain explicitly invalid under
`build/sdm-movhi-emitter/invalid-*.log`. None contributes to the witness.

## Next boundary

The existing dynamic postreload trace still establishes HI value forwarding
across the old-cursor copy and increment. The width lead supplies no supported
cast/source variant, because the binary opcode does not distinguish HI move
from explicit SI zero-extend. Likewise, the existing early UID38 lookup miss
does not predict a late widened read's value-table result: it precedes the
store. A hypothetical wide wrapper may miss simplify-set yet still expose
its HI input to operand substitution; neither outcome was dynamically tested.
Preserve the source and close this width hypothesis. A next experiment needs
an independent operand/use or alias-lifetime witness from the original graph,
not another interpretation of MOVZWL alone.

Playbook transfer: before inferring source promotion from zero/sign-extending
machine loads, consult the period backend's small-mode move emitter and a
real `-dP` pattern mapping. Register destination spelling and machine opcode
can reflect tune-dependent stall avoidance rather than an explicit source
conversion.

Integration checks: five stage checks/two distinct annotated MOVZWL patterns;
one installed emitter witness/three complete equal objects. `make refs` passes
14386 references/2965 finding headings and10038 static anchors. All300 production
object hashes are unchanged; the latest unchanged-source period gate is388/0.
No new source/runtime claim or compiler-profile control.
