/*
 * stubs.c -- the 21 imports `dsplibs.o` declares, so the blob can be linked
 * on its own.
 *
 * This probe deliberately does NOT link `$(OBJ)`.  The reconstruction defines
 * the same names the blob does, so the two cannot appear in one link, and the
 * rest of the suite solves that by renaming every blob symbol to `ref_*`.
 * Here the point is the opposite: the binary must contain the blob's symbols
 * under their REAL names, because that is the link `slmodemd` performs and
 * the one the fix has to work in.
 *
 * Nothing below is ever called -- the probe only reaches `FPM_sqrt` and
 * `FPM_div`, neither of which has an external dependency.  They exist so the
 * link resolves.
 */

int dsplibs_debug_level = 0;

void dsplibs_debug_printf(void) {}
void modem_debug_log_data(void) {}
void modem_dp_deregister(void) {}
void modem_dp_register(void) {}
void modem_get_bits(void) {}
void modem_get_param(void) {}
void modem_get_sreg(void) {}
void modem_put_bits(void) {}
void modem_recv_from_tty(void) {}
void modem_send_to_tty(void) {}
void modem_set_param(void) {}
void sysdep_free(void) {}
void sysdep_malloc(void) {}
void sysdep_memcpy(void) {}
void sysdep_memset(void) {}
void sysdep_sprintf(void) {}
void sysdep_strcat(void) {}
void sysdep_strcpy(void) {}
void sysdep_strlen(void) {}
void sysdep_vsnprintf(void) {}
