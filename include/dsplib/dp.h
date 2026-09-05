/*
 * dp.h -- DataPump: the interface slmodemd defines.
 *
 * These layouts are fixed by the existing ABI: slmodemd's modem.o is already
 * compiled against them, so they are reproduced exactly rather than
 * redesigned.  The originals are in slmodemd/modem_dp.h; they are duplicated
 * here so this tree builds standalone.
 *
 * The offsets matter to the reconstruction: dp_wrapper_run reaches the
 * wrapper state through `dp->dp_data`, which the disassembly shows at +0x10.
 */

#ifndef DSPLIB_DP_H
#define DSPLIB_DP_H

struct dp;
struct dp_operations;

/*
 * A datapump's process entry point, as stored by dp_wrapper_create and
 * invoked by dp_wrapper_run.  Returns 0 for success, or a DPSTAT_* code.
 */
typedef int (*dp_process_fn)(void *dp, void *in, void *out, int count);

struct dp_operations {
	const char *name;			/* +0x00 */
	int use_count;				/* +0x04 */
	struct dp *(*create)(void *modem, int id, int caller, int srate,
			     int max_frag, struct dp_operations *op);
	int (*destroy)(struct dp *dp);		/* `delete` in the original */
	int (*process)(struct dp *dp, void *in, void *out, int count);
	int (*hangup)(struct dp *dp);
};

struct dp {
	int id;					/* +0x00 enum DP_ID   */
	void *modem;				/* +0x04 struct modem */
	unsigned status;			/* +0x08 */
	struct dp_operations *op;		/* +0x0c */
	void *dp_data;				/* +0x10 */
};

/* Datapump status codes (slmodemd modem_defs.h). */
#define DPSTAT_OK         0
#define DPSTAT_CONNECT    1
#define DPSTAT_ERROR      4
#define DPSTAT_NODIALTONE 6
#define DPSTAT_BUSY       7
#define DPSTAT_NOANSWER   8
#define DPSTAT_CHANGEDP  10

/**
 * @brief Register every datapump's `dp_operations` table with the host.
 *
 * dsplibs' own export, not slmodemd's ABI: `modem_main.c` calls this once
 * to register the whole library and prop_dp_exit() once to tear it down.
 * See `src/core/dp_init.c`.
 *
 * @return 0 always -- the object is `xor %eax,%eax; ret`.
 */
int prop_dp_init(void);

/**
 * @brief The other half of prop_dp_init(). slmodemd declares this `void`;
 * the object returns 0 (`xor %eax,%eax; ret`) the same as prop_dp_init().
 * @return 0 always.
 */
int prop_dp_exit(void);

#endif /* DSPLIB_DP_H */
