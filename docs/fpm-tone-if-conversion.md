# FPM tone filter: destination identity transfers, exactness does not

Baseline5b1bed17. F11568 establishes GCC3.4's conditional-mask prerequisite:
zero versus the destination itself. This supplies new evidence to reopen the
specific ring conditional left unresolved by F11564's width-only controls.

Twelve complete-TU cells cross retained int taps/int idx, both short, or short
taps/int idx; outer int/short; original ternary or ordinary conditional update:

```c
idx = (short)(idx + 1);
if ((short)idx >= taps)
    idx = 0;
```

The signed-short pre-comparison wrap remains explicit. The mixed ring cell
separates the blob's word comparison from its wide index/pointer carrier.
No explicit mask, profile exception or declaration/register-order permutation.

| Ring carriers | Outer counter | Ternary bytes | Conditional bytes |
| --- | --- | ---: | ---: |
| retained int/int | int |238|238|
| retained int/int | short |239|237|
| short/short | int |239|233|
| short/short | short |240|234|
| short taps/int idx | int |239|233|
| short taps/int idx | short |240|234|

Reference235 bytes. All12 compile under Gentoo GCC3.4.2-r2 and its selected
assembler2.15.92.0.2, all source/object emissions distinct. Raw production
baseline reproduces; all four previous width controls raw-replay. Each
conditional ce1 reports1 possible block/1 converted, each ternary1/0. This
demonstrates the converter result, not merely a reduced size discrepancy.

All11 functions/12 global definitions and bindings preserved,4/11 exact
unchanged, no gains/losses. Only filter's canonical body changes; it has no
relocations. Data sections unchanged. The closest cells restore the20-byte
stack frame and mask/word comparison but still differ in incoming load order,
stack homes, instruction scheduling and final outer decrement/store sequence.
These are measured differences, not a proof that a unique original local type
has been recovered or that the remainder is exclusively register allocation.

No source adoption; candidate runtime/partial-link gates NOT RUN. Production
remains868/1852 exact and its prior fixed Gentoo phase385/0 is unchanged.
No mutation/fuzzing execution or modem-lifecycle claim. This finite
conditional/width transfer is closed; reopening needs independently established
value lifetime, owner/load factoring or profile evidence.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946012870).
Replay `tools/playbook_fpm_tone_ifcvt.py --domain URL`. Artifacts under
`build/playbook-fpm-tone-ifcvt` preserve saved .build-config-derived complete
commands and mandatory DSPLIB_REPRODUCE_BUGS, compiler/assembler identity,
source/header/object hashes, inventories, changed bodies and RTL dumps.
