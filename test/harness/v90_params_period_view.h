/* Reference-layout storage for ILP32 differential fixtures only. Native
 * component tests must use the actual V90Parameters member layout. */
#ifndef TEST_V90_PARAMS_PERIOD_VIEW_H
#define TEST_V90_PARAMS_PERIOD_VIEW_H
#include "dsplib/V90Parameters.h"
typedef char v90_period_view_requires_ilp32[sizeof(void *) == 4 ? 1 : -1];
/* Legacy ILP32 differential-fixture view. Production consumers use members;
 * this reference-layout view must never be used for native objects. */
union V90ParamsRaw {
	unsigned char	b[0x558];
	int		w[0x558 / 4];
	float		f[0x558 / 4];
};

/*
 * Guarded on a 32-bit pointer for the reason every offset assertion in this
 * tree is: the layout the blob has is a 32-bit layout, and `make check64`
 * lays the class out differently because it holds pointers.  Asserting the
 * equality on a host that cannot have it is asserting the wrong thing.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v90pr_size[(sizeof(union V90ParamsRaw)
			 == sizeof(V90Parameters)) ? 1 : -1];
#endif

#define V90PB(p)	(((union V90ParamsRaw *)(p))->b)
#define V90PW(p)	(((union V90ParamsRaw *)(p))->w)
#define V90PF(p)	(((union V90ParamsRaw *)(p))->f)


#endif
