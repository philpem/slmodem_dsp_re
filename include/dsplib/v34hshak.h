/*
 * v34hshak.h -- ITU-T V.34: the handshake's state names.
 *
 * `v34handshak` is 61,541 bytes holding one state machine, and this is its
 * eighty-seven states in the AUTHOR'S names, in index order.
 *
 * They come from `StateName`, a table of string pointers at .data+0x6c00,
 * and it is LIVE: `v34handshakinit` and `v34handshak` index it 533 times
 * between them, to print state transitions.  An earlier note here said
 * nothing indexed it, which was a relocation search missing an addend
 * against a section symbol -- see findings 144 and 152.
 *
 * The transitions name THREE concurrent machines, not one: a receive state,
 * a transmit state and a "microstate", each trace printing its own change
 * and the other two's current value.
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
 * The handshake's support functions -- everything in V34hshak.c that is not
 * `v34handshak` itself.  See src/pump/v34/v34hshak.c.
 */

#define V34_SCALE_ENTRIES	28	/* two rows of fourteen */
#define V34_CARRIER_DESC	8	/* four zeroes and two coefficient pairs */
#define V34_BPV22_TAPS		60

/*
 * The transmit power scales, indexed by pre-emphasis index; the receive
 * carrier descriptors; and the phase-2 DPSK band-pass pair.  All fifteen are
 * global in the object and referred to only from this translation unit.
 *
 * `bpv22high` and `bpv22low` are NOT const: they live in .data rather than
 * .rodata, which is the original's own statement about their storage class.
 */
extern const short scale2400[V34_SCALE_ENTRIES];
extern const short scale2800[V34_SCALE_ENTRIES];
extern const short scale3000[V34_SCALE_ENTRIES];
extern const short scale3200[V34_SCALE_ENTRIES];
extern const short scale3429[V34_SCALE_ENTRIES];

extern const short c1600[V34_CARRIER_DESC], c1680[V34_CARRIER_DESC];
extern const short c1800_[V34_CARRIER_DESC], c1829[V34_CARRIER_DESC];
extern const short c1867[V34_CARRIER_DESC], c1920[V34_CARRIER_DESC];
extern const short c1959[V34_CARRIER_DESC], c2000[V34_CARRIER_DESC];
extern const short c1200_[V34_CARRIER_DESC], c2400_[V34_CARRIER_DESC];

extern short bpv22high[V34_BPV22_TAPS];
extern short bpv22low[V34_BPV22_TAPS];

/*
 * Re-arm the FSK demodulator to look for INFO1.  Clears its working state
 * and reloads the V.21-rate slicer configuration; touches nothing else.
 */
void dpskDetectInfo1Init(void *obj);

/*
 * Bring the phase-2 DPSK link up.  `mode` zero selects a 1200 Hz transmit
 * carrier and anything else 2400; `high` non-zero selects the upper receive
 * band.  The two are independent -- the directions occupy different bands.
 */
void dpskinit(void *obj, short mode, short high);

/*
 * Turn the negotiated MP bit-fields at +0xa9de..+0xa9e3 into transmit and
 * receive symbol rates, carriers, power scales and a pre-emphasis index.
 * Rate codes 1, 6 and 7 set nothing at all.
 */
/*
 * Set the whole object up for phase 2.  Two configurations in one function,
 * chosen by `f359c == 0x65`: the originate end signals on 1200 Hz through
 * the upper receive band, the answer end on 2400 through the lower.
 */
void v34modeminit(void *obj);

void setfinalrate(void *obj);

/*
 * Configure the demodulator for the rate `setfinalrate` chose, and arm the
 * tone detector on the carrier descriptor it selected.
 */
void setupreceiver(void *obj);

/*
 * How many multiplications by a per-rate ratio it takes for a per-rate
 * measurement to pass a limit: the pre-emphasis index, 6..10.
 *
 * The object supplies no default arm, so a baud rate other than 2400, 2800,
 * 3000, 3200 or 3429 runs the loop on uninitialised registers there and on
 * zero here.  See D37.  What the first argument points at is not known --
 * nothing in the object calls this -- so it stays a `void *`.
 */
short preempindex(void *obj, short baudrate);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSHAK_H */
