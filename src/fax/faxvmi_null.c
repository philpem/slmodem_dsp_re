/*
 * faxvmi_null.c -- Class 1 fax: the null datapump and its message reporter.
 *
 * THE TRANSLATION UNIT IS RECOVERED (finding F11399).  The blob's FILE
 * record faxvmi_null.c sits between cTOOLS.c and reversedchars.c, and
 * `ld -r` concatenates `.text` in FILE order: cTOOLS.c's last function
 * `cTOOLS_handle_hdlc_output` ends at 0x09f0a3, these six functions fill
 * [0x09f0b0, 0x09f14b), and SDM.c's first function `SDM_scrambler` starts
 * at 0x09f150.  The bracket is exclusive and the function set matches one
 * for one, so all six are this FILE's.  `null_message` (0x09f140) had been
 * left in class1tx.c; it moves here verbatim.
 *
 * See nulldp.h for the address map, the dispatch tables these five belong to
 * (none of which is written here -- see nulldp.h for why), and where each
 * signature comes from.  In the object's own order:
 *
 *   null_create   .text 0x09f0b0   30
 *   null_delete   .text 0x09f0d0    1
 *   null_process  .text 0x09f0e0   59
 *   null_status   .text 0x09f120    6
 *   null_control  .text 0x09f130    6
 *   null_message  .text 0x09f140   11
 *
 * Differential test: test/unit/t_nulldp.c.
 */

#include <stddef.h>

#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"

void
null_create(struct faxvmi_link *dp, const void *cfg)
{
	(void)cfg;
	dp->int_0014 = 0;
	dp->pack_count = 0x32;
	dp->pack_width = 8;
	dp->unpack_width = 8;
}

void
null_delete(struct faxvmi_link *dp)
{
	(void)dp;
}

/*
 * The loop is the object's rotated `for`: a pre-test skips it entirely for
 * `*count <= 0` (0x09f0ec), matching the tail test that would otherwise stop
 * it after zero iterations.  `i` is 16-bit in the object -- `lea 0x1(%edx),
 * %eax` then `movswl %ax,%edx` re-narrows and re-sign-extends the index
 * after every increment, which a plain `int` would not need.
 */
int
null_process(struct faxvmi_link *dp, short *in, unsigned short *result,
	    unsigned short *count)
{
	unsigned short *dst = dp->ptr_0000;
	short i;

	(void)result;

	for (i = 0; i < *count; i++)
		dst[i] = (unsigned short)in[i];

	return -1;
}

int
null_status(struct faxvmi_link *dp, void *status)
{
	(void)dp;
	(void)status;
	return -1;
}

int
null_control(struct faxvmi_link *dp, void *arg)
{
	(void)dp;
	(void)arg;
	return -1;
}

void
null_message(void *handle, int code, char **out)
{
	(void)handle;
	(void)code;
	*out = NULL;
}
