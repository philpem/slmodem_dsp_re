# V23 transmit boundary cross

Base 240481e6; retained Gentoo profile and complete v23tx.c.

Original v23FP_tx_progress at 873b0 tests sample count for zero (8743d,
8748b) rather than signed positive, advances the output pointer in the
mute-clear loop (873e0), shares done/consumed publication, and never
initializes its captured bit before sampling. Retained source uses signed
positive iteration, an independent indexed mute-clear loop/early return,
and an artificial zero bit initializer with a live stack slot.

Cross these three independently visible boundaries, 8 complete TU cells
including unchanged raw control. Stream zero test preserves valid
nonnegative caller counts, while negative count behavior differs and is
recorded rather than claimed reachable. Removing the bit initializer
retains the object's undefined no-sample capture; no tests may invent valid
history on that unsupported path. The mute control uses existing done as
its loop counter and common publication, no artificial register carrier.

Prediction: original boundaries may eliminate extra capture spills and
recover the emitted function. Falsifier: any complete residual remains; no
size-only improvement is adopted. Preserve emitted bodies, definitions,
imports, data, and all bystanders; source adoption requires final period gate.
