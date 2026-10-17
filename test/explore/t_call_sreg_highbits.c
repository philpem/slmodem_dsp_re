/* Synthetic host-return probe: public constructor/process sequence, without
 * planted internal state. No evidence the modem host returns high-bit S-register
 * values. Byte-range public lifecycle cases remain in the normal t_call fixture.
 */
#define main included_call_main
#include "../unit/t_call.c"
#undef main

int main(void)
{
	static const unsigned values[] = {0x7fffffffu, 0x80000000u, 0xffffffffu};
	int field, value, rc = 0;
	for (field = 0; field < 2; field++)
	for (value = 0; value < 3; value++) {
		char label[96];
		sreg16 = field == 0 ? (long)values[value] : 1;
		sreg56 = field == 1 ? (long)values[value] : 2;
		sprintf(label, "synthetic host-return S%d bits=%08x", field ? 56 : 16, values[value]);
		rc |= run(label, "5", SIG_SILENCE, 8000, 48, 3);
	}
	return rc;
}
