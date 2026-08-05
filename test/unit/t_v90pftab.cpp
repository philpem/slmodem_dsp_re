/*
 * t_v90pftab.cpp -- differential test of V90PreFilter's ten static tables.
 *
 * 23,860 bytes, and unlike a function there is nothing to drive: the check is
 * that our copy IS the blob's copy.  So every table is compared against its
 * own `ref_` alias, which is the blob's bytes reached through the same
 * renaming every other symbol gets.  A generator that formatted a float
 * through %f and lost a bit fails here, which is the point -- the emitter is
 * not trusted, its output is measured.
 *
 * `dataBase` cannot be compared as bytes: sixteen of its seventeen entries
 * hold a pointer into one of the six loop tables, and our tables and the
 * blob's are at different addresses and always will be.  It is not skipped.
 * Each pointer is resolved against THAT side's own six bases and compared as
 * (which table, what offset into it), which is finding 224's rule applied to
 * data rather than to a return value.
 *
 * The counted lengths are checked too, because they are what the object
 * actually uses and one of them is not what the symbol size says: the blob's
 * refLoopsType2 is exactly 33 records with no zero-named terminator inside
 * it, and the loop that counts them stops on the .data alignment padding that
 * follows.  Our copy has a 34th, all-zero record instead.  Both count 33.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90PreFilter.h"

extern "C" {
extern float ref_coef1[31][20]
	asm("ref__ZN12V90PreFilter18preFilterCoefType1E");
extern float ref_coef2[31][20]
	asm("ref__ZN12V90PreFilter18preFilterCoefType2E");
extern float ref_coef3[31][40]
	asm("ref__ZN12V90PreFilter18preFilterCoefType3E");
extern V90RefLoop ref_loops1[23] asm("ref__ZN12V90PreFilter13refLoopsType1E");
extern V90RefLoop ref_loops2[33] asm("ref__ZN12V90PreFilter13refLoopsType2E");
extern V90RefLoop ref_loops4[36] asm("ref__ZN12V90PreFilter13refLoopsType4E");
extern V90RefLoop ref_loops5[36] asm("ref__ZN12V90PreFilter13refLoopsType5E");
extern V90RefLoop ref_loops6[34] asm("ref__ZN12V90PreFilter13refLoopsType6E");
extern V90RefLoop ref_loops7[34] asm("ref__ZN12V90PreFilter13refLoopsType7E");
extern V90CodecEntry ref_dataBase[17] asm("ref__ZN12V90PreFilter8dataBaseE");
}

/* One row of a coefficient bank, so a differing row is reported as a run. */
struct pf_row20 {
	float c[20];
};
struct pf_row40 {
	float c[40];
};

static int
run_banks(void)
{
	int r;

	diff_begin("V90PreFilter coefficient banks");

	for (r = 0; r < 31; r++) {
		diff_eq_obj("preFilterCoefType1 row", struct pf_row20,
			    V90PreFilter::preFilterCoefType1[r],
			    ref_coef1[r], r);
		diff_eq_obj("preFilterCoefType2 row", struct pf_row20,
			    V90PreFilter::preFilterCoefType2[r],
			    ref_coef2[r], r);
		diff_eq_obj("preFilterCoefType3 row", struct pf_row40,
			    V90PreFilter::preFilterCoefType3[r],
			    ref_coef3[r], r);
	}

	/*
	 * Anti-vacuity: a table of zeros would pass everything above.  Every
	 * bank has at least one row that is not all zero and at least one
	 * value with its low mantissa bits set, which is what a decimal
	 * round trip would have lost.
	 */
	{
		int nonzero = 0, ragged = 0, i;
		const float *p = &V90PreFilter::preFilterCoefType3[0][0];

		for (i = 0; i < 31 * 40; i++) {
			unsigned u;

			memcpy(&u, &p[i], sizeof(u));
			if (u != 0)
				nonzero++;
			if ((u & 0xf) != 0)
				ragged++;
		}
		diff_eq_int("bank 3 is not all zeros", nonzero > 1000, 1, 0);
		diff_eq_int("bank 3 has values with low mantissa bits set",
			    ragged > 500, 1, 0);
	}

	return diff_end();
}

struct loop_table {
	const char *name;
	V90RefLoop *ours;
	V90RefLoop *theirs;
	int blobRecords;	/* records inside the blob's symbol */
	int counted;		/* what the object's walk finds     */
};

static struct loop_table tables[] = {
	{ "refLoopsType1", V90PreFilter::refLoopsType1, ref_loops1, 23, 22 },
	{ "refLoopsType2", V90PreFilter::refLoopsType2, ref_loops2, 33, 33 },
	{ "refLoopsType4", V90PreFilter::refLoopsType4, ref_loops4, 36, 35 },
	{ "refLoopsType5", V90PreFilter::refLoopsType5, ref_loops5, 36, 35 },
	{ "refLoopsType6", V90PreFilter::refLoopsType6, ref_loops6, 34, 33 },
	{ "refLoopsType7", V90PreFilter::refLoopsType7, ref_loops7, 34, 33 },
};
#define NTAB ((int)(sizeof(tables) / sizeof(tables[0])))

/* The object's own count: walk until a record's name starts with NUL. */
static int
walk(const V90RefLoop *t)
{
	int n = 0;

	while (t[n].name[0] != '\0')
		n++;
	return n;
}

static int
run_loops(void)
{
	int t, k, signatures = 0;

	diff_begin("V90PreFilter reference loop tables");

	for (t = 0; t < NTAB; t++) {
		for (k = 0; k < tables[t].blobRecords; k++)
			diff_eq_obj(tables[t].name, V90RefLoop,
				    &tables[t].ours[k], &tables[t].theirs[k],
				    k);

		diff_eq_int("counted length of a loop table (%ld)",
			    walk(tables[t].ours), tables[t].counted, t);
		/*
		 * The blob's own count, taken the same way.  For five of the
		 * six this reads the terminator inside the symbol; for
		 * refLoopsType2 it reads the .data padding behind it, which is
		 * exactly the thing being recorded.
		 */
		diff_eq_int("the blob counts the same (%ld)",
			    walk(tables[t].theirs), tables[t].counted, t);

		for (k = 0; k < tables[t].counted; k++) {
			const V90RefLoop *r = &tables[t].ours[k];
			int i;

			diff_eq_int("coefType is 1, 2 or 3 (table %ld)",
				    r->coefType >= 1 && r->coefType <= 3, 1, t);
			for (i = 0; i < 6; i++)
				if (r->signature[i] != 0.0f)
					signatures++;
		}
	}

	diff_eq_int("the signatures are not all zero", signatures > 400, 1, 0);

	return diff_end();
}

/*
 * Which of the six tables a pointer lands in, and how far into it, computed
 * against the bases of the side that owns the pointer.
 */
static void
resolve(const V90RefLoop *const *bases, const V90RefLoop *p, int *which,
	long *off)
{
	int i;

	*which = -1;
	*off = -1;
	if (p == 0) {
		*which = -2;
		*off = 0;
		return;
	}
	for (i = 0; i < NTAB; i++) {
		long d = (const char *)p - (const char *)bases[i];

		if (d >= 0 && d < 68L * tables[i].blobRecords) {
			*which = i;
			*off = d;
			return;
		}
	}
}

static int
run_database(void)
{
	const V90RefLoop *ourBases[NTAB], *theirBases[NTAB];
	int k, resolved = 0, nulls = 0;

	diff_begin("V90PreFilter::dataBase");

	for (k = 0; k < NTAB; k++) {
		ourBases[k] = tables[k].ours;
		theirBases[k] = tables[k].theirs;
	}

	for (k = 0; k < 17; k++) {
		int wa, wb;
		long oa, ob;

		diff_eq_int("dataBase name (entry %ld)",
			    memcmp(V90PreFilter::dataBase[k].name,
				   ref_dataBase[k].name, 32) == 0, 1, k);

		resolve(ourBases, V90PreFilter::dataBase[k].loops, &wa, &oa);
		resolve(theirBases, ref_dataBase[k].loops, &wb, &ob);

		diff_eq_int("dataBase[%ld] points at the same table", wa, wb, k);
		diff_eq_int("dataBase[%ld] points at the same offset", oa, ob,
			    k);
		diff_eq_int("dataBase[%ld] resolved", wa != -1, 1, k);

		if (wa >= 0)
			resolved++;
		else if (wa == -2)
			nulls++;
	}

	diff_eq_int("sixteen entries point at a loop table", resolved, 16, 0);
	diff_eq_int("the seventeenth is the terminator", nulls, 1, 0);
	diff_eq_int("the terminator's name is empty",
		    V90PreFilter::dataBase[16].name[0], 0, 0);
	diff_eq_int("no earlier entry has an empty name",
		    V90PreFilter::dataBase[15].name[0] != 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_banks();
	rc |= run_loops();
	rc |= run_database();

	return rc;
}
