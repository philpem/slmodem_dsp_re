# Completion checks for source recovery

Literal object identity requires equal file bytes. Function identity and
link-relevant ELF comparison remain useful intermediate checks, with separate
denominators and failure conditions. Behavioural reconstruction still requires
the Gentoo period differential and `make phase`.

## Commands and exit statuses

Use complete, freshly validated reproduction objects and the original object:

```sh
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  python3 tools/toolchain/byteident.py --ratchet
python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o --json build/partial/comparison.json
python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o --require-exact
python3 tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o --require-raw-exact
```

The default partial comparison exits zero when its diagnostic census completes,
even if the files differ. `--require-exact` exits one for a difference in the
link-relevant comparison. `--require-raw-exact` exits one unless SHA-256 and a
direct comparison of all file bytes agree. Passing both flags requires both
results. Input errors and unsupported inputs fail rather than producing an
exact verdict.

The function ratchet preserves its recorded exact-name set. A passing ratchet
does not imply that every reference function is exact; inspect the full
reference/shared-name counts, all defining copies and unresolved targets.
Completion requires every reference function to be accounted for in addition
to the final file gate. `cmp` is an independent literal-byte check:

```sh
cmp ref/slmodemd/dsplibs.o build/partial/dsplibs.o
```

## What the semantic comparison retains

The comparator validates ELF32, little-endian i386 relocatable inputs and
compares ELF identification/version, OS ABI, ABI version and ELF flags. Its
section census includes every allocated section type and `.note.GNU-stack`,
whose flags affect executable-stack policy. The reported section count thus
includes one non-allocated policy section when that section exists.

Compared section records retain name, type, flags, address, size, alignment,
entry size, link and info. Section references are resolved to names rather
than relying on section-table indices. File-backed contents are compared at
the same positions; NOBITS extents are retained separately. Allocated
INIT_ARRAY contents receive the same checks as ordinary code and data.

Relocation-section metadata includes its actual target section, even if its
printed name suggests a different target. Each relocation also retains the
indexed destination symbol's type, binding, visibility, owner, value, size and
name. This distinguishes duplicate local names. Inline REL addends remain in
the compared section bytes. Parsed record counts must agree with readelf's
declared counts; unsupported or missing records cause refusal.

Defined and undefined symbols of every kind are compared in order, excluding
the null and SECTION bookkeeping records. SECTION records still identify
relocation destinations. Section groups retain their signature symbol, group
flags and ordered members, including non-allocated COMDAT group metadata.
Multiple symbol tables and duplicate compared section names are explicitly
unsupported. Duplicate non-allocated `.group` names are supported.

The semantic comparison deliberately permits differences in `.comment`,
unreferenced debug contents, file offsets and string-table packing. It is not
a complete model of every possible ELF extension. The raw gate covers every
byte, including omitted or unfamiliar metadata; all comparator modes retain
the input validation and explicit refusal rules. Non-allocated metadata,
section and symbol-table packing, ELF header packing and compiler records
therefore remain part of final recovery even when the semantic result is exact.

## Controlled correction of the measurement

Run the versioned controls through the usual host target:

```sh
make partial-compare-selftest
```

The completion controls invoke both public gates for each assembled pair.
They cover exact copies; changed code/data; merge entry size; added allocated
INIT_ARRAY and changed INIT_ARRAY contents; added undefined symbols; changed
undefined binding, type and visibility; section-link ownership; ELF flags;
relocation-section ownership; duplicate-name relocation targets; and COMDAT
group flags. A `.comment`-only change passes the semantic gate and fails the
raw gate. Two groups with duplicate `.group` names pass both gates when copied
unchanged. Metadata-only controls assert that allocated contents and symbol
records agree, so incidental drift cannot explain their rejection.

Before the implementation changed, the new initial 14-pair control set produced
25 failures in 28 expected gate decisions: 11 semantic omissions and 14 uses
of the absent raw option. A separate COMDAT-flag control also passed the old
semantic gate despite changed group-selection policy. The completed suite has
16 pairs and 32 correct CLI gate decisions. The existing eight comparisons
still pass, and four parser controls accept two supported inputs and refuse
two incomplete/unsupported relocation streams. These are apparatus controls;
they do not add recovered functions or prove modem reachability.

## Comparing old and new reports

JSON reports carry `comparison_schema: 2`. Symbol counts now include undefined
symbols, relocation matches include the complete indexed target identity, and
section records include the additional metadata. New `elf_abi`,
`relocation_sections` and `groups` dimensions contribute to semantic equality.
Regrade both candidates with the same comparator before comparing results.
Do not treat differences caused by the corrected model as source gains or
losses. The default diagnostic exit behaviour is unchanged.
