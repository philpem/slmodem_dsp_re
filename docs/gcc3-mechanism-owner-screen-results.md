# Owner-screen terminal diagnostic results

Revision 9f1199b57d3c91d1f26627eabd9eea496355798d. The read-only diagnostic inventory screened all 300 baseline objects, selected 91 C++/service objects, compared 960 defining function copies, and retained 310 strict nonexact copies. Three functions had an original terminal diagnostic JMP replaced by a CALL in reconstruction. Both historical V92 constructor positive controls fired. Each classified site is an R_386_PC32 relocation at an actual decoded instruction boundary; a byte resembling a CALL opcode inside an operand cannot fire. Counts are triage, not recovered source.

The finite ten-cell experiment reproduced all unchanged controls byte-for-byte with the recorded complete Gentoo3.4.2-r2 command and executed assembler identity. ADID matched restricted -dr controls avoid the previously measured full -da compiler ICE; that limitation is explicit. There are eight distinct source cells plus two deliberately repeated V90 raw/terminal controls.

One strict source gain: V92Modem::reset, 138 bytes. The illegal-side default arm's break is an explicit return in the winner overlay. At initial RTL neither arm has a selected sibling call; 02.sibling selects the diagnostic call as call_insn/j only with the explicit return, retained through 35.mach. All seven V92Modem functions become exact; the existing six exact neighbours are unchanged. The original folded disassembly has the diagnostic tail at 0x13cf0. This is the terminal-edge mechanism, not a late register-allocation accident.

V90Modem::reset baseline SIZE1 becomes SIZE2 under the same explicit return. The new diagnostic sibling edge matches the original but does not close the rest. The independent original probe-arm result witness was crossed: baseline/terminal/probe-local DilType/both produce SIZE1/2/3/2. No adoption. ADID findPadGain's final guarded-return control is raw-body inert at SIZE2; no adoption. No expansion to permutations or flag controls is justified by these results.

Whole-TU audits assert 130 strict body verdicts across ten complete-TU cells, identical symbol metadata/exports/named data/allocated nontext and canonical data relocations, and unchanged every bystander. Twenty-four stage controls establish the terminal call's first selection. Production source remains frozen; the parent decides integration and final batch gate.

Reproduction:

```
python3 tools/gcc3_mechanism_owner_screen.py
python3 tools/gcc3_mechanism_owner_screen_terminal.py --domain docs/gcc3-mechanism-owner-screen-terminal-domain.md --baseline-dir ../byteexact-batch100/build/tc_out
python3 tools/gcc3_mechanism_owner_screen_adid_terminal.py --domain docs/gcc3-mechanism-owner-screen-terminal-domain.md --baseline-dir ../byteexact-batch100/build/tc_out
python3 tools/gcc3_mechanism_owner_screen_v90_reset_result.py --domain docs/gcc3-mechanism-owner-screen-terminal-domain.md --baseline-dir ../byteexact-batch100/build/tc_out
python3 tools/gcc3_mechanism_owner_screen_terminal_audit.py
```

Durable artifact roots are build/gcc3-mechanism-owner-screen{.json,-terminal/,-adid-terminal/,-v90-reset-result/,-terminal-audit.json} and build/gcc3-mechanism-owner-dis/. Stable source winner is build/gcc3-mechanism-owner-screen-terminal/V92Modem/terminal-default-return/V92Modem.cpp; production delta is exactly its reset default break→return. Source retained-input/callback/union ages need paired operand tracking across calls, not this call-count detector; that broader screen remains a next task.

## Operand-age follow-through, five additional paired bodies

A manual paired folded-disassembly review supplies a separate five-function denominator; it is not an automated ownership proof and did not justify new source cells:

- V90Phase4Modulator::setMappingParams: original child +0x44 is read for reset and reloaded after reset for setSymbolsBlockSize. Source and baseline already do both reads. Diagnostic sibling edge already matches; remaining BYTES10 involves register carriers.
- V92Phase4Modulator::setMappingParams: same original two-age child ownership at +0x6c (0x17854 before reset, 0x17868 after reset); source/baseline match it. Diagnostic tail already present. BYTES8 does not identify a missing owner boundary.
- RD_create: original retains entry modem through allocation/zeroing and passes that retained input to modem_get_param at 0x21d0, instead of reloading published rd->modem. Source matches the retained input. No parameter/member control is warranted by the BYTES11 residual.
- detector_create: original acquires get_sreg from its incoming stack slot after the four TONE_create calls, then calls through that input and uses the original modem input for both cadence creations. Baseline does the same. The cfg copy/returned child store schedule differs, but no external call separates those stores; this is not a new callback-age witness and no order fitting was attempted.
- silence_progress is an exact calibration body for mutable callback ownership: original and baseline both call query via object +4 at entry and again in each silent-block path. The later call reuses no callback snapshot from entry. Source already expresses the member-owned callback at each use.

All ten additional folded original/baseline listings are retained in build/gcc3-mechanism-owner-dis/. Thus the current automated terminal census is exhaustive for its stated 91-TU pool; operand-age follow-through covers five additional functions only. Existing union-view-width opportunities were not re-enumerated by this call-site inventory. An alias-aware inter-call operand classifier requires a separately declared mechanism/proof scope rather than guessing from load counts.
