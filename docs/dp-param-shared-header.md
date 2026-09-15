# `dsp_info::clock_deviation`: shared-header experiment

This note records the isolated period-compiler experiment for spelling
`struct dsp_info::clock_deviation` as the host spells it: `long`, rather than
the reconstruction header's `int`.  It is evidence for the shared-header work
tracked by issue #15.  It does not by itself settle the LP64 representation of
this 32-bit boundary type.

## Independent provenance

`third_party/slmodem/modem_defs.h` is a verbatim, hash-pinned Smart Link host
header.  It declares the four-member host record as:

```c
struct dsp_info {
	unsigned connection_type;
	long     clock_deviation;
	unsigned qc_lapm;
	unsigned qc_index;
};
```

The object independently fixes four 32-bit accesses at offsets 0, 4, 8 and
12.  On the period i386 ABI, `int` and `long` are both four bytes and have the
same alignment.  The object therefore cannot distinguish the declarations by
access width or layout.  The host declaration is strong provenance for
`long`; a code-generation improvement is corroboration, not proof from score
alone.

On LP64, native `long` instead puts `clock_deviation` at offset 8, the two
following words at 16 and 20, and makes the structure 24 bytes.  The blob's
layout remains 16 bytes.  Integrating the host declaration therefore needs a
separate explicit portability decision; the 32-bit reconstruction result does
not establish that the blob used an LP64-native layout.

## Experiment construction

All experiment artifacts are under `build/dp-param-shared-header/`.  No
versioned source or header was changed for the measurement.

The baseline was `build/tc_repro`, whose manifest contains 275 objects.  Its
`.deps` records identify 26 translation units that include
`include/dsplib/modem_params.h`; `affected.tsv` records the exact object/source
mapping.  Each candidate began as the complete baseline object set, then only
those 26 objects were recompiled through an overlay copy of the header.

The control overlay left the declaration unchanged.  The candidate overlay
changed only:

```c
int clock_deviation;
```

to:

```c
long clock_deviation;
```

Both arms used the retained flags, with the C++-only flags on `.cpp` files and
the DCR exception available for `src/service/dcr.c` (which is not in the
affected set).  `DSPLIB_REPRODUCE_BUGS` was appended last.  The shared
experiment helper was called with the published Gentoo compiler path and
`native=True`, because its image-name detector does not recognize the
published registry name.

The measured tools were:

```
image       ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
gcc         /usr/i386-pc-linux-gnu/gcc-bin/3.4/gcc
version     GCC 3.4.2 (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
target      i386-pc-linux-gnu
assembler   GNU assembler 2.15.92.0.2 20040927
```

## Results

The unchanged-header control reproduced all 26 affected baseline objects byte
for byte.  The `long` overlay changed exactly one of the 275 objects:
`src_core_dp_param.c.o`.  The other 25 header consumers and the 249 copied
non-consumers remained byte-identical.

Within that object only `dp_runtime_create` changed.  `dp_param_get` and
`dp_runtime_delete` did not.  `dp_runtime_create` remained 298 bytes; its
symbol binding, visibility, section, value and size were unchanged.  The
complete object symbol tables are identical.

The change is instruction scheduling.  In particular, the candidate moves the
load of `info->clock_deviation` earlier among the stores to the runtime block,
toward the object's placement.  Comparing the 298-byte function bodies at
equal positions gives:

| arm | bytes differing from the blob |
|---|---:|
| baseline `int` | 53 |
| candidate `long` | 28 |

The candidate has 25 fewer differing positions.  Baseline and candidate
differ at 60 positions; this is a reordering, not a size change.

The full 1,852-symbol `byteident.py` census and exact membership are unchanged:

| grade | baseline | candidate |
|---|---:|---:|
| EXACT | 830 | 830 |
| unresolved as exact | 4 | 4 |
| REGALLOC | 48 | 48 |
| BYTES | 80 | 80 |
| SIZE | 890 | 890 |

There are zero exact gains and zero exact losses.  The mnemonic census is also
unchanged: 890 identical, 72 same-size different, and 890 different-size.
`dp_runtime_create` remains non-exact, so the result is a partial-fidelity
improvement supported by independent host provenance, not closure of the
function.

The complete 275-object candidate was partially linked in the order recorded
by `build/tc_repro/tc_link_manifest.txt`, using the published image's `ld`
2.15.92.0.2.  Comparing it with the retained canonical baseline reports all
99 section records, all 17,144 relocations and all 2,959 symbol records
identical.  Sixty positioned `.text` bytes differ; all other positioned bytes
agree.

Against the blob, the canonical partial-link positioned-byte count moves from
68,456 in the retained baseline to 68,457 in the candidate: a gain of one.
This whole-object result is consistent with the isolated function result, but
the 25-position function-local improvement is the more specific measurement.

An initial invalid link used `tc_manifest.txt`, which records compile-manifest
order rather than canonical link order.  It produced 50,797 baseline and
50,795 candidate equal positioned bytes.  Those numbers are retained under
`build/dp-param-shared-header/partial/` as experiment history but must not be
quoted as fidelity results; changing input order changed unrelated layout.

## Gate status

The Gentoo period differential gate was **not run as part of this experiment**.
This note describes an overlay-only compiler measurement.  Any retained header
change must separately pass the repository's normal period gate and record its
test denominator.
