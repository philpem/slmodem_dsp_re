/*
 * v32state.h -- ITU-T V.32/V.32bis: the handshake's state names.
 *
 * Thirty-five states in the AUTHOR'S names, from the `statenames` table at
 * .rodata+0x7e80.  `V32StateName(int)` is its accessor and bounds-checks
 * against 0x22, which is exactly the last index -- so the table length is
 * confirmed by the code rather than inferred from the symbol size.
 *
 * The lettering is the RECOMMENDATION'S, not the author's invention: V.32
 * labels its call-setup states A through Z, and the doubled ones (B2, D2,
 * F2, X2) are the spec's own sub-states.  That makes this table a direct
 * index into the standard, which is worth more than a set of invented
 * names would be.
 *
 * V.32 is not reconstructed -- one of the six build stamps in finding 135
 * belongs to its translation unit (V32FP_recreate, 15:48:07), so it exists
 * in the object and has simply not been reached yet.  Written down now
 * because the table costs nothing to read and would otherwise be
 * rediscovered.  See finding 145.
 */

#ifndef DSPLIB_V32STATE_H
#define DSPLIB_V32STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define V32_STATE_A             0
#define V32_STATE_B             1
#define V32_STATE_B2            2
#define V32_STATE_C             3
#define V32_STATE_D             4
#define V32_STATE_D2            5
#define V32_STATE_E             6
#define V32_STATE_F             7
#define V32_STATE_G             8
#define V32_STATE_H             9
#define V32_STATE_I            10
#define V32_STATE_J            11
#define V32_STATE_K            12
#define V32_STATE_L            13
#define V32_STATE_M            14
#define V32_STATE_N            15
#define V32_STATE_O            16
#define V32_STATE_P            17
#define V32_STATE_Q            18
#define V32_STATE_R            19
#define V32_STATE_S            20
#define V32_STATE_T            21
#define V32_STATE_U            22
#define V32_STATE_V            23
#define V32_STATE_W            24
#define V32_STATE_X            25
#define V32_STATE_Y            26
#define V32_STATE_Z            27
#define V32_STATE_END          28
#define V32_STATE_F2           29
#define V32_STATE_X2           30
#define V32_STATE_CLEARDOWN    31
#define V32_STATE_DONE         32
#define V32_STATE_ERROR        33
#define V32_STATE_DONT_CARE    34

#define V32_STATE_COUNT		35

/* statenames[state] for state <= 34, else "STATE_UNKNOWN". */
const char *V32StateName(int state);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32STATE_H */
