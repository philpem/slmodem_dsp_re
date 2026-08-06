/*
 * t_v34pcmtab.c -- the V.34 PCM module's data tables against the blob's.
 *
 * Compared through the `ref_` alias, which is the blob's own bytes reached
 * under a different name, so this is a differential check and not a
 * self-consistency one.  `V34DisconnectThreshTable` is file-local in the
 * object and is only aliasable because the Makefile globalizes file-local
 * symbols before renaming them.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v34pcm_tables.h"

extern const int ref_V34DisconnectThreshTable[V34_DISCONNECT_THRESH_ENTRIES]
	asm("ref_V34DisconnectThreshTable");

int
main(void)
{
	int i, bad;

	diff_begin("V34DisconnectThreshTable");

	for (i = 0; i < V34_DISCONNECT_THRESH_ENTRIES; i++)
		diff_eq_int("entry %ld", V34DisconnectThreshTable[i],
			    ref_V34DisconnectThreshTable[i], i);

	/*
	 * The whole object too, not just the entries: a table one entry short
	 * or one entry long compares equal on every entry it has.
	 */
	diff_eq_int("all %ld bytes",
		    memcmp(V34DisconnectThreshTable,
			   ref_V34DisconnectThreshTable,
			   sizeof(V34DisconnectThreshTable)) == 0, 1,
		    (long)sizeof(V34DisconnectThreshTable));
	diff_eq_int("the table is %ld bytes",
		    (long)sizeof(V34DisconnectThreshTable), 32, 32);

	/*
	 * Strictly increasing, which is what makes it a ladder and what a
	 * transcription error in the middle would break without changing the
	 * ends.  Independent of the comparison above on purpose.
	 */
	for (i = 1; i < V34_DISCONNECT_THRESH_ENTRIES; i++)
		diff_eq_int("entry %ld is above the one before it",
			    V34DisconnectThreshTable[i]
			    > V34DisconnectThreshTable[i - 1], 1, i);

	bad = diff_end();
	return bad;
}
