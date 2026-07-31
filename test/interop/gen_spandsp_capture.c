/*
 * gen_spandsp_capture.c -- freeze a SpanDSP signal for the 32-bit harness.
 *
 * The interop tier is 64-bit (SpanDSP) and the differential tier is 32-bit
 * (the blob), so they cannot share a process.  To ask whether the blob loses
 * lock on SpanDSP's signal the way the reconstruction does, the signal has to
 * cross that boundary as data.
 *
 * Writes two files:
 *   spandsp_b103.pcm   int16 samples, 8 kHz, SpanDSP's Bell 103 CH2 (the
 *                      answerer's 2025/2225, which an originating station
 *                      receives)
 *   spandsp_b103.bits  one byte per bit, the data that went in
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spandsp.h>

#define NBITS   5000
#define NBLOCKS 700

static unsigned char bits[NBITS];
static int pos;

static int
get_bit(void *user)
{
	(void)user;
	return pos < NBITS ? bits[pos++] : 0;
}

int
main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : ".";
	char path[512];
	fsk_tx_state_t *tx;
	FILE *fp;
	unsigned lfsr = 0x2B1Du;
	short air[256];
	int i, n, blocks = 0;

	if (preset_fsk_specs[FSK_BELL103CH2].freq_zero != 2025) {
		fprintf(stderr, "wrong SpanDSP: BELL103CH2 is not 2025/2225.\n"
			"This is 0.0.6, which has the channels swapped.\n");
		return 1;
	}

	for (i = 0; i < NBITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		bits[i] = (unsigned char)(lfsr & 1);
	}

	tx = fsk_tx_init(NULL, &preset_fsk_specs[FSK_BELL103CH2], get_bit, NULL);
	if (tx == NULL)
		return 1;

	snprintf(path, sizeof(path), "%s/spandsp_b103.pcm", dir);
	fp = fopen(path, "wb");
	if (fp == NULL) {
		perror(path);
		return 1;
	}
	while (blocks < NBLOCKS && pos < NBITS - 200) {
		n = fsk_tx(tx, air, 160);
		if (n <= 0)
			break;
		fwrite(air, sizeof(short), (size_t)n, fp);
		blocks++;
	}
	fclose(fp);

	snprintf(path, sizeof(path), "%s/spandsp_b103.bits", dir);
	fp = fopen(path, "wb");
	if (fp == NULL) {
		perror(path);
		return 1;
	}
	fwrite(bits, 1, (size_t)pos, fp);
	fclose(fp);

	printf("wrote %d blocks (%d samples) and %d bits to %s\n",
	       blocks, blocks * 160, pos, dir);
	return 0;
}
