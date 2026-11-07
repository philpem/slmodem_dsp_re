#ifndef DSPLIB_V34_TX_CONTROL_H
#define DSPLIB_V34_TX_CONTROL_H

#include "dsplib/v34rx.h"

/* Native txinit, txmit, v34modeminit and settxlevel retain root+0x221c
 * independently of the modem root. This complete measured control group
 * includes the 230-word queue and all observed scalars through root+0x25dc.
 * Its two tail bytes are alignment; the original type name and whether its
 * declaration continued beyond this measured group remain unresolved. */
struct v34_tx_control {
#include "dsplib/v34_tx_control_fields.h"
};

#endif
