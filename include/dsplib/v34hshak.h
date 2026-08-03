/*
 * v34hshak.h -- ITU-T V.34: the handshake's state names.
 *
 * `v34handshak` is 61,541 bytes holding one state machine, and this is its
 * eighty-seven states in the AUTHOR'S names, in index order.
 *
 * They come from `StateName`, a table of string pointers at .data+0x6c00.
 * Nothing in this object indexes it -- the debug call sites that printed it
 * were compiled out or live elsewhere -- but the table survived, and a name
 * table survives a printf's removal because it is separately addressable
 * data.  See findings 134 and 144.
 *
 * Nothing here is reconstructed yet; tasks #39-#45 are.  The point of
 * writing them down first is that planning the reconstruction against
 * "state 47" and against "TX_PHASE3_ANS" are very different exercises, and
 * cfgsplit's per-state byte counts become readable the moment the states
 * have names.
 *
 * The gaps are the author's: NOSTATE0, NOSTATE2, NOSTATE3 and NOSTATE36 are
 * placeholders, so the enum has four holes and eighty-three real states.
 */

#ifndef DSPLIB_V34HSHAK_H
#define DSPLIB_V34HSHAK_H

#ifdef __cplusplus
extern "C" {
#endif

#define V34HS_NOSTATE0                0
#define V34HS_TXRENEG                 1
#define V34HS_NOSTATE2                2
#define V34HS_NOSTATE3                3
#define V34HS_RECEIVE                 4
#define V34HS_SILENCE                 5
#define V34HS_ANSAM                   6
#define V34HS_TONE_2100               7
#define V34HS_TONE2225                8
#define V34HS_AA_TX                   9
#define V34HS_CC_TX                  10
#define V34HS_AC_TX                  11
#define V34HS_CA_TX                  12
#define V34HS_SXMIT                  13
#define V34HS_XMIT1                  14
#define V34HS_XMIT2                  15
#define V34HS_XMIT3                  16
#define V34HS_XMITV22                17
#define V34HS_SSEG                   18
#define V34HS_SBARSEG                19
#define V34HS_PPSEG                  20
#define V34HS_TRNSEG4                21
#define V34HS_TRNSEG16               22
#define V34HS_TX_JM_CM               23
#define V34HS_TX_DPSK                24
#define V34HS_DET_2100               25
#define V34HS_DET_2250               26
#define V34HS_DET_2400               27
#define V34HS_DET_1200               28
#define V34HS_DET_AC                 29
#define V34HS_DET_AC_RTN             30
#define V34HS_DET_AC_END             31
#define V34HS_DET_AA                 32
#define V34HS_PHASE1                 33
#define V34HS_PHASE2                 34
#define V34HS_WAIT                   35
#define V34HS_NOSTATE36              36
#define V34HS_RECEIVE1               37
#define V34HS_RECEIVE2               38
#define V34HS_RECEIVEV22             39
#define V34HS_DET_CM_JM              40
#define V34HS_DET_SYNC               41
#define V34HS_DET_CJ                 42
#define V34HS_RX_DPSK                43
#define V34HS_DET_INFO               44
#define V34HS_TONE_AB_ANS            45
#define V34HS_TX_PHASE1_ANS          46
#define V34HS_TX_PHASE2_ANS          47
#define V34HS_TX_PHASE3_ANS          48
#define V34HS_RX_PHASE1_ANS          49
#define V34HS_RX_PHASE2_ANS          50
#define V34HS_TX_L1                  51
#define V34HS_TX_L2                  52
#define V34HS_DET_AB                 53
#define V34HS_SILENCEINFO            54
#define V34HS_TX_PHASE1_CALL         55
#define V34HS_TX_PHASE2_CALL         56
#define V34HS_TX_PHASE3_CALL         57
#define V34HS_RX_PHASE1_CALL         58
#define V34HS_RX_PHASE2_CALL         59
#define V34HS_TONE_AB                60
#define V34HS_TONE_AB_CALL           61
#define V34HS_RX_PHASE3_CALL         62
#define V34HS_INFODONE               63
#define V34HS_JTXMIT                 64
#define V34HS_XMIT0                  65
#define V34HS_TRNSEG4A               66
#define V34HS_XMITMP                 67
#define V34HS_J1TXMIT                68
#define V34HS_EXMIT                  69
#define V34HS_DATAXMIT               70
#define V34HS_TXLEVEL                71
#define V34HS_RX_L1                  72
#define V34HS_RX_L2                  73
#define V34HS_SILENCERETRAIN         74
#define V34HS_RX_RETRAIN_CALL        75
#define V34HS_RX_RETRAIN_ANSWER      76
#define V34HS_TX_RETRAIN_ANS         77
#define V34HS_JaTXMIT                78
#define V34HS_MOH_TONE               79
#define V34HS_MOH_TONE_DROP          80
#define V34HS_MOH_SILENCE            81
#define V34HS_MOH_ON_HOLD            82
#define V34HS_MOH_FRR                83
#define V34HS_MOH_CLEARDOWN          84
#define V34HS_K56JaTXMIT             85
#define V34HS_TXMD                   86

#define V34HS_STATE_COUNT		87

/*
 * ---------------------------------------------------------------------------
 * The handshake's two symbol emitters, and the constellations they map onto.
 *
 * Both scramble the bits handed to them, encode the result differentially
 * against the quadrant the object is carrying, and TAIL-CALL `txmit` -- so
 * calling either of these transmits a symbol rather than merely computing
 * one.  Everything they touch is in `struct v34_object`.
 *
 * Each table entry is a packed complex int, real part in the low half.
 */
extern const int vect4[4];
extern const int vect16[16];

/* Two bits -> one quadrant, differentially against the last. */
void txmitdibit(void *obj, short bits);

/*
 * Four bits -> one of sixteen points.  The low dibit picks the quadrant,
 * differentially; the high one selects within it and is not.
 */
void txmitquadbit(void *obj, short bits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSHAK_H */
