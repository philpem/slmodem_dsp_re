# Issue #260 wave 11 (closure) — working state

Branch `cleanup/issue260-census` @ 443bf1d9. Everything UNCOMMITTED (task says do not commit).

## Measured baseline (offsetcensus at HEAD)
787 sites: castadd 250, helper 386, byteview 151. Tree-wide total at wave 0: 1712.
Castadd+byteview: V34hshak 111, Data.c 61, VPcmV34Main 48, arms 32, class1rx 21,
v34info 12, V34RX 12, cHDLCrx 10, class1 9, class1tx.h 8, V32stc 7, v34shell 6.

## Mutation-surface constraints (verified against test/mutations/*.json)
- `#define T3M_F0E4C 0x0e4c` is mutated by v34hstxblock.json -> KEEPS ITS LITERAL.
- `#define DP_RX_BLOCK_CAP`, DP_TIMER_* mutated -> keep (values, not offsets).
- TX1_COUNT/TX1_FAA86/TX1_FA244/TX1_F25D6/TX1_INFOREC/TX1_FAA80/TX1_HS_MODE/
  TX1_F358C/TX1_F359E SITE texts are mutated (v34hstx1.json, v34hstx1_moved.json,
  v34hst3core.json) -> converting those sites means RE-RECORDING those suites.
- v34hshak.json (stale, issue #180) has 18 mutants touching V34hshak.c castadd
  sites; suite stays stale per the brief.
- v34datapump.json mutates dp_rxput/hs_get(obj, DP_FAA98) sites -> NOT touching
  datapumpv34 (F8044 boundary), so that suite survives untouched.
- F637: a define turned into offsetof makes a textual define-mutation unusable;
  the only define-line mutation among our constants is T3M_F0E4C (kept literal).

## Region recoveries (v34fsk.h), evidence per member
1. +0x254..+0x25c: dc_count (unsigned short, +0x254, countdown init
   10000+336*short_35a4), dc_est (short, +0x256, "Estimated DC = %d"), dc_acc
   (int, +0x258, "(acc = %d)"). Strings cb868; O_ comment block; OB_F0254 sites.
2. +0xa244: phase3_hd_length (int; string cb950 "phase3halfDuplexLength = %d
   symbols (baud %d)"); +0xa248 hd_set (short; usage inference: set with the
   length by one arm, cleared by three). +0xa242 stays 2 unmapped bytes.
3. +0xe4c: modem_up (short; "set to 1 by each of the three modem is up arms",
   usage inference). unmapped_0a00 split around it.
4. +0xaa40..+0xaa6c: the sixth message record after info_caps/caps_flags
   (= word[0]/word[1]): msg_word[8] (+0xaa40), msg_crc (+0xaa50), msg_crc_on,
   msg_nbits, msg_pos, msg_wordbits, msg_idx, msg_repeat, msg_repeats,
   msg_acc (int, +0xaa60 = TX1_FAA60), msg_avail, msg_avail0,
   msg_acc0 (int, +0xaa68 = TX1_FAA68). Measured fit: every tx1_ts_rates/MOH
   write lands exactly on a v34_bitsource member (crc=-1, crc_on=1,
   wordbits=0x10, acc/acc0=0x3fffe ints).
5. ratecfg pad_16 -> pe_2400 (+0x16), pe_2800 (+0x18), short_aa9a (+0x1a, no
   access), pe_3000 (+0x1c), pe_3200 (+0x1e), pe_3429 (+0x20) — probeselect's
   own local names.
6. +0xabc4 pcm_chosen (short; usage), +0xabd4 v90_high_carrier (short; the
   create copies V92Phase2Info::v90UseHighCarrier into it; usage inference),
   +0xabe9 ansam_late (unsigned char; "ANSam detected on out going call (later
   case...)").
7. modem_params.h: unnamed_0003 -> flags3 (consumer's CFG_FLAGS3; bits: 4 =
   retrain, 2 = phase2), unnamed_0051 -> flags51 (the +0xac40 request mailbox's
   flag byte).
8. v34recv.h: receiver pad_264 -> short_264 (arm 49 writes it, t72_probe_done
   byte-reads it; T3M_RX_F264 keeps its literal, byte view recorded).

## Constant anchors (wave-2 form, free)
- v34hstx1_arms.h: HS_TRACE_2/TX1_COUNT -> short_aa78; TX1_F358C -> short_358c;
  TX1_HS_MODE -> hs_mode; TX1_FAA7A -> short_aa7a; TX1_FAA7C -> filtdelay;
  TX1_FAA86 -> ratecfg+period; TX1_FAAE0/FAAE2 -> fsk.nbits/fsk.sr; TX1_F3590
  -> short_3590; TX1_F3598 -> short_3598; TX1_F359E -> short_359e; TX1_F359A
  -> force_low_baud; TX1_FA244 -> phase3_hd_length; TX1_FAA80 ->
  rate_change_pending; TX1_FABE4/FABE6 -> short_abe4/short_abe6; TX1_RATEIDX ->
  ratecfg.rxbits; TX1_TXRATEIDX -> ratecfg.txbits; TX1_INFOREC -> msgrec base
  + sizeof(bitsource); TX1_FA97E -> msgrec[1].word[1]; TX1_FAA3C -> info_caps
  (byte view); TX1_RX_PRED -> receiver pred_b; TX1_MOH_ACTIVE -> moh_active;
  TX1_FAA60 -> msg_acc; TX1_FAA68 -> msg_acc0.
  KEPT LITERAL (no member): TX1_F25D6/F25D8/F25DA (unmapped_25d6), TX1_RXCOEFF
  (unmapped_0e84's receive coefficient twin), T3M_F0E4C (mutated define).
- V34hshak.c: T3M_COUNTER/T3C_COUNT -> short_aa78; T3C_COUNT_SRC/T3M_FILTDELAY
  -> filtdelay; T3M_TOGGLE -> short_358c; T3M_FABAE -> short_abae; T3M_FABC2 ->
  short_abc2; T3C_MODE -> hs_mode; T3M_F3588/F358A -> short_3588/short_358a;
  T3C_FAAE2 -> fsk.sr anchor (F553 comment stays); T4_MPCAPS -> info_caps
  anchor; T44_PROBE -> probe_bins; DP_RX_* -> struct v34_receiver members
  (conversion itself DECLINED per F8044/F8045); DP_FAA98 -> ratecfg.rxbits;
  V34_MSGREC_BASE -> offsetof(msgrec); T3M_RX_F264 keeps literal (byte view of
  receiver short_264). T41_FABE8 -> moh_active.
- VPcmV34Main.cpp: O_P2STATE -> p2_state (sites stay PROG_U8: zero-extending
  reads of a signed byte); OB4_V90_RECEIVER/K56_RECEIVER -> offsetof minus
  OB4_ANCHOR; O_NOTCH_* -> retrainReqDet members (as needed after conversion).
- v34hshak.h V34HS_*_OFF stay literals: cannot spell offsetof there (v34fsk.h
  includes v34hshak.h, struct not yet defined); the HS_OFF_ASSERT trio in
  V34hshak.c is the compile-time anchor (F632/F637 mechanism).
- v34info.c SESSION_*: members EXIST in VPcmFloModem.h (pcmSessionType +0x611c,
  info0Layout +0x6120, v92modem.phase2Info +0x612c, V90Modem's phase2Info
  +0x1760) but the TU is C and the session is a C++ class behind void* ->
  recorded, not converted.
- K56_AORMU/K56_ENABLED: K56FlexFloModem has NO data members -> keep offsets.

## Site conversions (object base, type-exact width+sign)
- arms: tx1_get/tx1_put(o, TX1_COUNT/TX1_FAA86/TX1_FAA7A/TX1_FAA7C/TX1_F358C/
  TX1_F3590/TX1_F3598/TX1_F359E/TX1_F359A/TX1_FAA80/TX1_FABE4/TX1_FABE6)
  -> members; tx1_get_int/tx1_put_int(o, TX1_HS_MODE)
  -> o->hs_mode; TX1_TXRATEIDX -> o->ratecfg.txbits; TX1_RATEIDX ->
  o->ratecfg.rxbits; TX1_FA244 -> o->phase3_hd_length; TX1_FAAE0/FAAE2 puts ->
  o->fsk.nbits/o->fsk.sr; TX1_FAA60/FAA68 ints -> o->msg_acc/o->msg_acc0;
  TX1_MOH_ACTIVE byte reads -> o->moh_active.
  KEPT: TX1_F25D6/8/DA helpers, TX1_RXCOEFF castadd, TX1_INFOREC castadd,
  TX1_FA97E byte view, TX1_FAA3C byte views.
- V34hshak.c: v34modeminit (retrain five -> members; retrain_bins[i] dftbin
  fields; 0x3588/0x358a/0xaa78/0x2aa0/0x2aa2 -> members); setfinalrate/
  setupreceiver/probeselect (ratecfg members incl. pe_*; role); settxlevel
  (tx_scale); v34setuptxmit (msgrec[3].word[0]); v34handshakinit (hs_mode,
  paa6c/paa70 stores, msgrec[1].word[0], short_ac12/short_ac14); fsk_clear
  (fsk_interp[i]); t53_rx_det_ab (&obj->msgrec[3]); t3c_getb(obj, T41_FABE8)
  trio -> obj->moh_active.
- VPcmV34Main.cpp: O_RUNNING -> samples_valid; O_MOHLIMIT/O_MOHCOUNT ->
  moh_limit/moh_timer; OB_MOH_FLAG -> moh_path_sel; OB_MOH_W4/W6 ->
  short_abe4/short_abe6; OB_F2218 -> hs_mode; O_ANSAMLATE -> ansam_late;
  O_P2STATE sites STAY (helper); O_PCMCHOSEN/OB_FABC4 -> pcm_chosen;
  O_HDSET/OB_FA248 -> hd_set; O_HDLENGTH -> phase3_hd_length; O_DCCOUNT/
  O_DCEST/O_DCACC/OB_F0254 -> dc_count/dc_est/dc_acc; O_MODEMUP -> modem_up;
  O_NOTCH_* -> retrainReqDet.y1/y2/x1/(x2 unsigned site stays or use-site
  cast)/notchDetectSigCnt/b1_q14/a1_q14/a2_q14/energyInp/energyOut/nsamples;
  CFG_FLAGS3 -> pac3c->flags3; CFG_FLAGS2 -> qcFlags; CFG_FLAGS51 -> flags51;
  CFG_SILENCE -> addedDelay; cfg+0x44 -> anchored CFG_ constant (short pun of
  an int member, aliasing); PROG_S32(obj, 0x244) -> int_0244;
  PROG_U8(phase2Info, 0x10) -> shortPhase2Local.
- v34info.c: nothing converted (record).
- OUT OF SCOPE (wave-8/9 records stand): V34RX.c, v34shell.c, v34filters.c,
  V34TX.c, Data.c, fax files, V32stc, v22mod, Dialer, V90AutoDigitalImpDetector.
- DELETIONS: SESS_V92_P2INFO (zero uses); hs_get/hs_put deleted if orphaned.

## Verification plan
1. nohup make phase J=10 -> period differential: N passed, M failed (zero).
2. Byte-identity set-check vs 443bf1d9, SEQUENTIAL tc builds (w13base in
   /tmp/opencode), byteident --json-out both sides, exact_symbols compare.
3. mutsnap re-records at --jobs 10 for suites whose locators changed
   (v34hstx1, v34hstx1_moved, v34hst3core, + any other affected); v34hshak
   stays stale (issue #180).
4. refcheck clean; final census vs wave 0's 1712.
## Progress log (wave 11 execution)
- Struct recoveries applied in v34fsk.h (dc trio, phase3_hd_length/hd_set,
  modem_up, sixth record msg_*, ratecfg pe_* slots, pcm_chosen,
  v90_high_carrier, ansam_late); modem_params.h unnamed_0003/unnamed_0051
  KEPT offset-named after measuring the blast radius: five V.90/V.34 TUs and
  four of their mutation suites key on those spellings -- renaming is a
  cross-service change of its own, recorded for a follow-up.
- v34recv.h: receiver pad_264 -> short_264.
- Define anchors applied (arms header, V34hshak.c, VPcmV34Main.cpp); literals
  kept: T3M_F0E4C (mutated define), T4_PLLCNT/T4_MPCOEF, TX1_F25D6/F25D8/
  F25DA, TX1_RXCOEFF, OB_F000C, O_CLR_*, SESSION_* (C TU, C++ members),
  K56_AORMU/K56_ENABLED, V34HS_*_OFF (header precedes the struct; anchored by
  the HS_OFF_ASSERT trio instead).
- Sites converted object-base/type-exact; tx1_get_int/tx1_put_int deleted
  (zero callers); hs_put deleted (zero callers; hs_get keeps its three
  datapumpv34 callers per the F8044 boundary).
- ERROR FOUND AND FIXED BY THE FIXTURE: TX1_INFOREC's first anchoring spelled
  msgrec[1] (the arms header's own stale comment says "the second"); the
  value 0xaa0c is msgrec[4], which is what t_v34hstx1's TRNSEG4 case caught
  (the MP CRC landed in the wrong record). The define is
  offsetof(msgrec[4]) now and the comment records the correction.
- Mutation re-records: v34hstx1 (521 caught/22 equivalent/0 unusable --
  identical to HEAD), v34hstx1_moved, v34hst3core (70 caught, 0 unusable),
  v34hst3m41 (84 caught, 2 equivalent), v34hsrx72 (124 caught), v34hst3mid;
  six mutant texts repaired where the mechanical rewrite left a stray paren
  or a dead helper; v34hshak.json stays stale per the brief (issue #180).

## Final state (wave 11)
- make phase: period differential: 388 passed, 0 failed; phase boundary all OK
  (run four times while anchor repairs landed; final run green end to end).
- Byte-identity set-check vs HEAD 443bf1d9 (w13base base tc, branch tc,
  sequential): 1852 compared, 1056 exact, exact_bytes 114695 both sides --
  ZERO exact symbols lost, ZERO gained. Re-run after the last source edit
  (remote_retrain_ind) with the same result.
- refcheck: 14397 references checked, 0 unresolvable, exit 0.
- Census: 787 -> 550 raw-offset sites (wave 0 tree-wide total: 1712);
  castadd 250 -> 180, helper 386 -> 221, byteview 151 -> 149.
- Mutation verdicts re-recorded for v34hstx1 (521 caught/22 equivalent/0
  unusable), v34hstx1_moved (240/1/0), v34hst3core (70/0/0), v34hst3m41
  (84 caught/2 equivalent/0 unusable), v34hsrx72 (124/1/0), v34hst3mid
  (419 caught/2 NOT caught [pre-existing at HEAD]/1 unusable/21 equivalent),
  v34pcmif (120/6/0), v34retrain (67/7/0), vpcmcreate (29/2/0) -- all
  identical to HEAD's recorded guarantees. v34hshak.json's locators were
  refreshed (0xac17, settxlevel, role, paa6c/paa70, retrain-reset context)
  but the suite stays stale per the brief (issue #180).
- NOTE: one `make tc` invocation ran in the MAIN tree by mistake (cwd); it
  rebuilt the main tree's build/tc_out objects (root-owned, regenerable
  derived artifacts; the main tree's sources were not touched). All tc
  measurements quoted here come from worktree builds re-run afterwards.
