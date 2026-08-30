/*
 * voice.h -- the voice modem's leaves, as far as this batch reaches.
 *
 * Voice is OPEN and lowest priority (CLAUDE.md); what is here is the one
 * `voice.c` symbol from finding F8320's no-entry-point bucket that has no
 * unreconstructed dependency.  `voice_set_online` and `voice_set_duplex`
 * were in the same batch's scope and are left out: each installs an
 * unreconstructed handler (`voice_online` / `voice_duplex`) and calls
 * `detector_set_enable`, all three of which the voice service reaches --
 * so they wait for the voice pass rather than dragging it in.  Finding
 * F8493.
 */

#ifndef DSPLIB_VOICE_H
#define DSPLIB_VOICE_H

/*
 * Map a DLE event code to a status code: 1 -> 10, 2 -> 11, 4 -> 12, and
 * ANY other code answers the first argument back unchanged -- the caller's
 * running status, passed in so the non-event is a no-op.
 */
int _handle_status(int status, int code);

#endif /* DSPLIB_VOICE_H */
