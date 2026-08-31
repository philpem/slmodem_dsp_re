/*
 * silence.h -- the silence detector's object, plus `_status`, which is its
 * neighbour in the blob rather than its relative.
 *
 * The five symbols sit together at the end of the `Fdspkrnl.c` span:
 * _status 0xb02e0, silence_is_more_then 0xb0360, silence_create 0xb03b0,
 * silence_delete 0xb0410, silence_progress 0xb0420.  Four of them are here;
 * silence_progress is not reconstructed yet -- it needs the LOCAL `.data`
 * table at 0x84d4 that the blob calls `silence_level_table`.
 *
 * Everything but `count` keeps a neutral name: silence_create is the only
 * writer reconstructed so far and it writes constants, so nothing here says
 * what the two ints it stores are FOR.
 */

#ifndef DSPLIB_SILENCE_H
#define DSPLIB_SILENCE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `sizeof` is 0x14 -- what silence_create asks sysdep_malloc for.  The two
 * leading ints are silence_create's second and third arguments, stored and
 * not otherwise read by anything reconstructed.
 */
struct silence {
	int		int_00;		/* +0x00 silence_create's arg 2      */
	int		int_04;		/* +0x04 silence_create's arg 3      */
	unsigned short	count;		/* +0x08 the run length that
					 *       silence_is_more_then judges */
	short		short_0a;	/* +0x0a zeroed at create            */
	short		short_0c;	/* +0x0c zeroed at create            */
	unsigned char	pad_0e[2];	/* +0x0e                             */
	int		int_10;		/* +0x10 zeroed at create            */
};

/*
 * Initialise `s`, allocating it when NULL -- and, unlike FIFO8_create,
 * checking that allocation: a failure returns NULL rather than faulting.
 */
struct silence *silence_create(struct silence *s, int a, int b);

/* Free the object. */
void silence_delete(struct silence *s);

/*
 * Is the accumulated run longer than `ms` worth of it?
 *
 * The object multiplies `ms` by 10.0f and truncates toward zero before the
 * comparison, so the unit of `count` is a tenth of whatever unit `ms` is --
 * nothing reconstructed increments `count`, so which it is stays open.
 */
int silence_is_more_then(struct silence *s, float ms);

/*
 * Append a two-byte DLE escape -- 0x10 then `code` -- at `*out`, and add 2
 * to `*len`.  It is `_status` in the blob and it is not part of the silence
 * detector; it lives here because it is the same TU's neighbour and has
 * nowhere better to go until its callers (voice_rx, voice_set_rx) land.
 *
 * The debug line the object prints for it is "DLE %d", with `code` promoted
 * from a signed char -- which is what types the third parameter.
 */
void _status(unsigned char *out, unsigned short *len, char code);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_SILENCE_H */
