/* Shared apparatus for the three VPcm graph fixtures. Include after reg/base
 * declarations. Types come from explicit pointer paths, never differing bytes.
 * Each hop must exist at the same offset in the same paired allocation. */
static long classified_float_words, classified_total_words;

static int float_pair(const void *a, const void *b)
{
	int k;
	for (k = 0; k < nreg; ++k)
		if (reg[k].a == a && reg[k].b == b)
			return k;
	return -1;
}

static int float_hop(int parent, unsigned off)
{
	void *a, *b;
	int child;
	if (parent < 0)
		return -1;
	if (off > reg[parent].size || sizeof a > reg[parent].size - off) {
		diff_eq_int("float path offset valid", 0, 1, off);
		return -1;
	}
	memcpy(&a, reg[parent].a + off, sizeof a);
	memcpy(&b, reg[parent].b + off, sizeof b);
	if (!a && !b)
		return -1; /* optional path absent on BOTH sides */
	child = float_pair(a, b);
	diff_eq_int("float path paired allocation", child >= 0, 1, off);
	return child;
}

static void float_mark_pair(int k, unsigned off, unsigned count)
{
	if (k < 0)
		return;
	if (count == 0 && off <= reg[k].size)
		count = (reg[k].size - off) / 4;
	if (off % 4 || reg[k].size % 4 || off > reg[k].size ||
	    count > (reg[k].size - off) / 4 || !count || reg[k].fcount) {
		diff_eq_int("float span valid and unique", 0, 1, k);
		return;
	}
	reg[k].foff = off;
	reg[k].fcount = count;
}

static void mark_float_graph(unsigned echo, unsigned demod, unsigned modem)
{
	int root = float_pair(base[0], base[1]);
	int arma, k;
	diff_eq_int("float graph root paired", root >= 0, 1, 0);
	float_mark_pair(float_hop(root, echo + 0x24), 0, 0);
	arma = float_hop(root, echo + 0x04);
	float_mark_pair(arma, 0x2c, 2);
	float_mark_pair(float_hop(arma, 0x0c), 0, 0);
	float_mark_pair(float_hop(float_hop(root, demod), 0x98), 0, 0);
	float_mark_pair(float_hop(float_hop(float_hop(root, modem), 0x50), 0x04), 0, 0);
	for (k = 0; k < nreg; ++k) {
		classified_float_words += reg[k].fcount;
		classified_total_words += reg[k].size / 4;
	}
}
