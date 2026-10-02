# V.34 minimum-level configuration reload control

F11662 records the two-cell domain announced in [issue #22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957533657) before compilation, at b75131e2. The complete VPcmV34Main.cpp translation unit is compiled using the saved production .build-config, Gentoo GCC 3.4.2-r2 and its executed binutils 2.15.92.0.2 assembler, with DSPLIB_REPRODUCE_BUGS enabled. The raw baseline reproduces production.

The blob's VPcmV34SetMinimumSigLevel stores the selected threshold at 0x639f and reads the captured configuration pointer's +0x60 field again at 0x63b8 for the diagnostic. Our baseline passes the cached level. The sole candidate replaces that diagnostic argument with a fresh int read through the same captured cfg pointer. Initial threshold selection, guards, stores, types, layouts and compiler profile are unchanged.

| Cell | Function bytes | Strict exact functions in TU |
| --- | ---: | ---: |
| Baseline | 98 | 23/57 |
| Diagnostic reload | 101 | 23/57 |
| Blob | 104 | — |

The source predicate succeeds: initial RTL, combine and allocated instruction RTL contain one configuration +96 read in the baseline and two in the candidate. Candidate disassembly stores the threshold at +0x230 before its diagnostic reload from cfg+0x60. The complete body still fails strict comparison; the blob also carries a root+4 base that this experiment does not explain. No independently typed four-byte header owner has been established. Do not fit that difference with an invented layout, volatility or register/declaration variants.

The complete-object audit checks all 57 functions and one named data object. Only this function changes. Symbol types, bindings, visibility, imports/exports, named data, allocated nontext sizes/contents and their canonical relocations agree between cells. There are no exact gains or losses. Two valid compilations produce two distinct raw emissions.

Reproduce with:

```sh
python3 tools/playbook_v34_minlevel_reload.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957533657
python3 tools/playbook_v34_minlevel_audit.py
```

Artifacts are in build/playbook-v34-minlevel-reload, including results.json and complete-object-audit.json. The stage audit deliberately excludes allocation diagnostic listings before counting actual allocated instructions.

Existing test/unit/t_v34pcmapi.cpp run_siglevel coverage has 25 paired direct component calls: 18 debug-off levels, five live-debug cases and two debug-threshold controls. It compares the owner/configuration objects, threshold selection, +0x234 clearing and live transcripts. These calls use separate configuration storage; they do not prove reachable configuration/owner aliasing. No candidate runtime or alias-equivalence claim is made.

No reconstruction source is adopted. The two-cell family is closed despite its successful local prediction. Production remains 925/1852 strict exact; the last deciding fixed period/structural gate remains 386 passed, zero failed. This evidence-only update does not rerun fuzzing or mutation harnesses.
