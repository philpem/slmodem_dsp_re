/*
 * b103.h -- Bell 103 / V.21 datapump.
 *
 * 300 bit/s FSK, full duplex, no equaliser.  The two standards differ only in
 * their tone frequencies, so one implementation serves both and registers
 * under both DP_IDs.
 */

#ifndef DSPLIB_B103_H
#define DSPLIB_B103_H

#include "dsplib/dp.h"

/* Datapump identifiers, from slmodemd's enum DP_ID (modem_defs.h). */
#define DP_V21  21
#define DP_B103 103

extern struct dp_operations b103_ops;

int dp_b103_init(void);
void dp_b103_exit(void);

#endif /* DSPLIB_B103_H */
