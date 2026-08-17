/*
 * Focused probe for SmartLink V92CP bit-vector packing.
 *
 * This is the next boundary below V.8: V8Update can select DP_V92 when the
 * six-word V.92/PCM preamble is detected, but the answer-side V.8 constructor
 * does not produce that preamble. V92CP::infoToBits() is a later V.92 control
 * packet packer, so this probe records stable blob outputs before replacing
 * that module.
 */

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "sl_v92_cp.h"

#define V92CP_BYTES 0x1000U

extern void V92CP_ctor(void *self) __asm__("_ZN5V92CPC1Ev");
extern void V92CP_infoToBits(void *self) __asm__("_ZN5V92CP10infoToBitsEv");
extern uint32_t V92CP_bitsToInfo(void *self, uint8_t bit)
	__asm__("_ZN5V92CP10bitsToInfoEh");
extern uint8_t *V92CP_getBitVector(void *self, uint32_t *len)
	__asm__("_ZN5V92CP12getBitVectorERj");
extern void V92CP_setSUV(void *self, uint32_t suv)
	__asm__("_ZN5V92CP6setSUVEj");
extern void setV92CPpckFromParamsInfo(void *params, void *request,
				      void *cp);
extern void setParamsInfoFromV92CPUnPck(void *params, void *cp);

static uint32_t dword_at(const uint8_t *base, unsigned off)
{
	uint32_t v;

	memcpy(&v, base + off, sizeof(v));
	return v;
}

static void put_dword(uint8_t *base, unsigned off, uint32_t v)
{
	memcpy(base + off, &v, sizeof(v));
}

static float float_at(const uint8_t *base, unsigned off)
{
	float v;

	memcpy(&v, base + off, sizeof(v));
	return v;
}

static void put_float(uint8_t *base, unsigned off, float v)
{
	memcpy(base + off, &v, sizeof(v));
}

static uint32_t float_bits(float v)
{
	uint32_t bits;

	memcpy(&bits, &v, sizeof(bits));
	return bits;
}

static uint16_t word_at(const uint8_t *base, unsigned off)
{
	uint16_t v;

	memcpy(&v, base + off, sizeof(v));
	return v;
}

static void put_word(uint8_t *base, unsigned off, uint16_t v)
{
	memcpy(base + off, &v, sizeof(v));
}

static void dump_bits(const char *name, const uint8_t *bits, unsigned count)
{
	unsigned i;

	printf("%s bits=", name);
	for (i = 0; i < count; i++) {
		if (i && (i % 8U) == 0)
			putchar('_');
		putchar(bits[i] ? '1' : '0');
	}
	putchar('\n');
}

static int compare_source_stream_decode(const char *name,
					const struct sl_v92_cp_info *expected,
					const struct sl_v92_cp_bits *source);

static int compare_source(const char *name, const uint8_t *blob_bits,
			  uint32_t len, uint32_t write_pos,
			  uint32_t padded_len, const uint8_t *cp)
{
	struct sl_v92_cp_info info;
	struct sl_v92_cp_info unpacked;
	struct sl_v92_cp_bits source;
	struct sl_v92_cp_bits repacked;
	int rc;
	int ok;

	memset(&info, 0, sizeof(info));
	info.state = cp[0];
	info.repeat = cp[1];
	info.info_byte = cp[2];
	info.section_tag = cp[3];
	info.tail_flag = cp[4];
	info.suv = dword_at(cp, 0x108);
	info.state1_code = word_at(cp, 0x104);
	info.session_flags = dword_at(cp, 0x8);
	info.scalar_flags = dword_at(cp, 0xc);
	info.scalar_magnitude = float_at(cp, 0x10);
	for (unsigned i = 0; i < 4U; i++)
		info.signed_scalar[i] = float_at(cp, 0x14 + i * 4U);
	for (unsigned i = 0; i < 4U; i++)
		info.short_scalar_a[i] = word_at(cp, 0x28 + i * 4U);
	for (unsigned i = 0; i < 2U; i++)
		info.short_scalar_b[i] = word_at(cp, 0x38 + i * 4U);
	info.pad_repeat = cp[0x128];
	info.descriptor_count = word_at(cp, 0x10c);
	info.secondary_enabled = cp[0x24];
	for (unsigned group = 0; group < SL_V92_CP_MAX_DESCRIPTOR_GROUPS;
	     group++) {
		for (unsigned word = 0; word < SL_V92_CP_WORDS_PER_DESCRIPTOR;
		     word++) {
			unsigned idx = group * SL_V92_CP_WORDS_PER_DESCRIPTOR + word;

			info.primary_descriptor[group][word] =
				word_at(cp, 0x42 + idx * 2U);
			info.secondary_descriptor[group][word] =
				word_at(cp, 0xa2 + idx * 2U);
		}
	}
	rc = sl_v92_cp_pack_recovered(&info, &source);
	if (rc == -ENOTSUP) {
		printf("%s source_compare=unsupported\n", name);
		return 0;
	}
	ok = rc == 0 && source.bit_count == len &&
		source.write_pos == write_pos &&
		source.padded_count == padded_len &&
		memcmp(source.bits, blob_bits, len) == 0;
	printf("%s source_compare=%s\n", name, ok ? "ok" : "FAIL");
	if (!ok && rc == 0) {
		printf("%s source len=%u write=%u padded=%u\n", name,
		       source.bit_count, source.write_pos, source.padded_count);
		dump_bits(name, source.bits, source.bit_count);
	}
	if (!ok)
		return 1;
	ok = sl_v92_cp_unpack_recovered(&unpacked, source.bits,
					source.write_pos) == 0 &&
		sl_v92_cp_pack_recovered(&unpacked, &repacked) == 0 &&
		repacked.write_pos == source.write_pos &&
		memcmp(repacked.bits, source.bits, source.write_pos) == 0;
	printf("%s source_unpack=%s\n", name, ok ? "ok" : "FAIL");
	if (ok)
		ok = compare_source_stream_decode(name, &info, &source) == 0;
	return ok ? 0 : 1;
}

static unsigned inferred_source_descriptor_count(const struct sl_v92_cp_info *info)
{
	unsigned max_index = 0;

	if (info->state != 0 || info->repeat > 1U)
		return info->descriptor_count;
	for (unsigned i = 0; i < 4U; i++) {
		if (info->short_scalar_a[i] > max_index)
			max_index = info->short_scalar_a[i];
	}
	for (unsigned i = 0; i < 2U; i++) {
		if (info->short_scalar_b[i] > max_index)
			max_index = info->short_scalar_b[i];
	}
	return max_index + 1U;
}

static int compare_source_stream_decode(const char *name,
					const struct sl_v92_cp_info *expected,
					const struct sl_v92_cp_bits *source)
{
	struct sl_v92_cp_decoder decoder;
	unsigned inferred_tables = inferred_source_descriptor_count(expected);
	int status = SL_V92_CP_DECODE_PENDING;

	sl_v92_cp_decoder_reset(&decoder, expected->pad_repeat);
	for (unsigned i = 0; i < source->write_pos; i++)
		status = sl_v92_cp_decoder_feed(&decoder, source->bits[i]);
	if (expected->state == 0 && expected->repeat <= 1U &&
	    expected->descriptor_count < inferred_tables) {
		int ok = status == SL_V92_CP_DECODE_PENDING &&
			decoder.expected_write_pos > source->write_pos;

		printf("%s source_stream=%s expected_write=%u write=%u\n",
		       name, ok ? "pending-table-ok" : "FAIL",
		       decoder.expected_write_pos, source->write_pos);
		return ok ? 0 : 1;
	}
	int ok = status == SL_V92_CP_DECODE_COMPLETE &&
		decoder.expected_write_pos == source->write_pos;

	if (ok) {
		struct sl_v92_cp_bits repacked;

		ok = sl_v92_cp_pack_recovered(&decoder.info, &repacked) == 0 &&
			repacked.write_pos == source->write_pos &&
			memcmp(repacked.bits, source->bits,
			       source->write_pos) == 0;
	}
	if (ok && source->write_pos > 1U) {
		struct sl_v92_cp_decoder truncated;
		struct sl_v92_cp_decoder corrupt;
		int truncated_status = SL_V92_CP_DECODE_PENDING;
		int corrupt_status = SL_V92_CP_DECODE_PENDING;

		sl_v92_cp_decoder_reset(&truncated, expected->pad_repeat);
		for (unsigned i = 0; i + 1U < source->write_pos; i++)
			truncated_status =
				sl_v92_cp_decoder_feed(&truncated,
						       source->bits[i]);
		sl_v92_cp_decoder_reset(&corrupt, expected->pad_repeat);
		for (unsigned i = 0; i < source->write_pos; i++) {
			uint8_t bit = source->bits[i];

			if (i == source->write_pos - 2U)
				bit ^= 1U;
			corrupt_status = sl_v92_cp_decoder_feed(&corrupt, bit);
		}
		ok = truncated_status == SL_V92_CP_DECODE_PENDING &&
			corrupt_status == SL_V92_CP_DECODE_ERROR;
	}
	printf("%s source_stream=%s expected_write=%u write=%u\n",
	       name, ok ? "ok" : "FAIL", decoder.expected_write_pos,
	       source->write_pos);
	return ok ? 0 : 1;
}

static int compare_decoded_blob_fields(const uint8_t *decoded,
				       const uint8_t *expected)
{
	if (decoded[0] != expected[0] ||
	    decoded[1] != expected[1] ||
	    decoded[2] != expected[2] ||
	    decoded[4] != expected[4])
		return 0;
	if (decoded[0] == 1)
		return word_at(decoded, 0x104) == word_at(expected, 0x104) &&
			dword_at(decoded, 0x108) == dword_at(expected, 0x108);
	if (expected[1] > 1)
		return 1;
	if (decoded[3] != expected[3])
		return 0;
	if (dword_at(decoded, 0x8) != dword_at(expected, 0x8) ||
	    dword_at(decoded, 0xc) != dword_at(expected, 0xc) ||
	    dword_at(decoded, 0x10) != dword_at(expected, 0x10) ||
	    decoded[0x24] != expected[0x24] ||
	    word_at(decoded, 0x10c) != word_at(expected, 0x10c))
		return 0;
	for (unsigned i = 0; i < 4U; i++) {
		if (dword_at(decoded, 0x14 + i * 4U) !=
		    dword_at(expected, 0x14 + i * 4U))
			return 0;
	}
	for (unsigned i = 0; i < 6U; i++) {
		if (dword_at(decoded, 0x28 + i * 4U) !=
		    dword_at(expected, 0x28 + i * 4U))
			return 0;
	}
	return 1;
}

static uint16_t reverse16(uint16_t v)
{
	uint16_t out = 0;

	for (unsigned i = 0; i < 16U; i++)
		out |= (uint16_t)(((v >> i) & 1U) << (15U - i));
	return out;
}

static unsigned inferred_decode_descriptor_count(const uint8_t *expected)
{
	unsigned max_index = 0;

	if (expected[0] != 0 || expected[1] > 1)
		return word_at(expected, 0x10c);
	for (unsigned i = 0; i < 6U; i++) {
		unsigned index = dword_at(expected, 0x28 + i * 4U);

		if (index > max_index)
			max_index = index;
	}
	return max_index + 1U;
}

static int compare_decoded_blob_descriptor_fields(const uint8_t *decoded,
						  const uint8_t *expected)
{
	for (unsigned group = 0; group < SL_V92_CP_MAX_DESCRIPTOR_GROUPS;
	     group++) {
		for (unsigned word = 0; word < SL_V92_CP_WORDS_PER_DESCRIPTOR;
		     word++) {
			unsigned idx = group * SL_V92_CP_WORDS_PER_DESCRIPTOR + word;

			if (word_at(decoded, 0x42 + idx * 2U) !=
			    reverse16(word_at(expected, 0x42 + idx * 2U)))
				return 0;
			if (word_at(decoded, 0xa2 + idx * 2U) !=
			    reverse16(word_at(expected, 0xa2 + idx * 2U)))
				return 0;
		}
	}
	return 1;
}

static int compare_blob_decode(const char *name, const uint8_t *bits,
			       uint32_t write_pos, const uint8_t *expected)
{
	uint8_t decoded[V92CP_BYTES];
	uint32_t ret = 0;
	unsigned expected_tables = inferred_decode_descriptor_count(expected);
	unsigned emitted_tables = word_at(expected, 0x10c);

	memset(decoded, 0, sizeof(decoded));
	V92CP_ctor(decoded);
	decoded[0x128] = expected[0x128] ? expected[0x128] : 1;
	for (uint32_t i = 0; i < write_pos; i++)
		ret = V92CP_bitsToInfo(decoded, bits[i]);
	if (expected[0] == 0 && expected[1] <= 1 &&
	    emitted_tables < expected_tables) {
		int pending_ok = dword_at(decoded, 0x114) == 7U &&
			word_at(decoded, 0x10c) == expected_tables;

		printf("%s blob_decode=%s ret=%u detector_state=%u "
		       "write=%u inferred_tables=%u emitted_tables=%u\n",
		       name, pending_ok ? "pending-table-ok" : "FAIL", ret,
		       dword_at(decoded, 0x114), dword_at(decoded, 0x11c),
		       expected_tables, emitted_tables);
		return pending_ok ? 0 : 1;
	}
	int ok = dword_at(decoded, 0x114) == 10U &&
		compare_decoded_blob_fields(decoded, expected) &&
		compare_decoded_blob_descriptor_fields(decoded, expected);
	printf("%s blob_decode=%s ret=%u detector_state=%u write=%u\n",
	       name, ok ? "ok" : "FAIL", ret, dword_at(decoded, 0x114),
	       dword_at(decoded, 0x11c));
	return ok ? 0 : 1;
}

static int compare_blob_leading_noise_boundary(const char *name,
					       const uint8_t *bits,
					       uint32_t write_pos,
					       const uint8_t *expected)
{
	uint8_t decoded[V92CP_BYTES];
	static const uint8_t noise[] = { 0, 1, 0, 1, 1, 0, 0, 1 };
	uint32_t ret = 0;

	memset(decoded, 0, sizeof(decoded));
	V92CP_ctor(decoded);
	decoded[0x128] = expected[0x128] ? expected[0x128] : 1;
	for (unsigned i = 0; i < sizeof(noise); i++)
		ret = V92CP_bitsToInfo(decoded, noise[i]);
	for (uint32_t i = 0; i < write_pos; i++)
		ret = V92CP_bitsToInfo(decoded, bits[i]);
	int ok = dword_at(decoded, 0x114) != 10U;

	printf("%s blob_leading_noise=%s ret=%u detector_state=%u write=%u\n",
	       name, ok ? "not-resyncing" : "FAIL", ret,
	       dword_at(decoded, 0x114), dword_at(decoded, 0x11c));
	return ok ? 0 : 1;
}

static int compare_unpacked_params(const char *name,
				   const struct sl_v92_cp_info *info,
				   const uint8_t *cp)
{
	uint8_t blob_params[0x700];
	struct sl_v92_cp_unpacked_params source_params;
	int rc;
	int ok;

	memset(blob_params, 0, sizeof(blob_params));
	setParamsInfoFromV92CPUnPck(blob_params, (void *)cp);
	rc = sl_v92_cp_params_from_info(&source_params, info);
	ok = rc == 0 &&
		dword_at(blob_params, 0x000) == source_params.info_source &&
		dword_at(blob_params, 0x61c) == source_params.secondary_enabled &&
		dword_at(blob_params, 0x620) == source_params.session_flags &&
		dword_at(blob_params, 0x624) == source_params.scalar_flags;
	for (unsigned i = 0; ok && i < 4U; i++) {
		ok = dword_at(blob_params, 0x628 + i * 4U) ==
			float_bits(source_params.signed_scalar[i]);
	}
	for (unsigned group = 0; ok && group < SL_V92_CP_MAX_DESCRIPTOR_GROUPS;
	     group++) {
		ok = dword_at(blob_params, 0x638 + group * 4U) ==
			source_params.source_to_unique[group] &&
			dword_at(blob_params, 0x604 + group * 4U) ==
			source_params.code_count[group] &&
			memcmp(blob_params + 0x004 + group * 0x80U,
			       source_params.primary_codes[group],
			       source_params.code_count[group]) == 0 &&
			memcmp(blob_params + 0x304 + group * 0x80U,
			       source_params.secondary_codes[group],
			       source_params.code_count[group]) == 0;
	}
	printf("%s params_unpacked=%s\n", name, ok ? "ok" : "FAIL");
	return ok ? 0 : 1;
}

static int run_case(const char *name, void (*prepare)(uint8_t *))
{
	uint8_t cp[V92CP_BYTES];
	uint32_t len = 0;
	uint8_t *bits;
	uint32_t write_pos;
	uint32_t padded_len;
	unsigned dump_len;
	int ok;

	memset(cp, 0, sizeof(cp));
	V92CP_ctor(cp);
	if (prepare)
		prepare(cp);

	V92CP_infoToBits(cp);
	bits = V92CP_getBitVector(cp, &len);
	write_pos = dword_at(cp, 0x11c);
	padded_len = dword_at(cp, 0x90c);
	dump_len = len;
	ok = bits == cp + 0x129 && len != 0 && padded_len >= write_pos;

	printf("%s len=%u write=%u padded=%u state=%u repeat=%u suv=%u "
	       "tail_flag=%u result=%s\n",
	       name, len, write_pos, padded_len, cp[0], cp[1],
	       dword_at(cp, 0x108), cp[0x4], ok ? "ok" : "FAIL");
	dump_bits(name, bits, dump_len);
	if (ok)
		ok = compare_source(name, bits, len, write_pos, padded_len, cp) == 0;
	if (ok)
		ok = compare_blob_decode(name, bits, write_pos, cp) == 0;
	if (ok && write_pos <= 290U)
		ok = compare_blob_leading_noise_boundary(name, bits, write_pos,
							 cp) == 0;
	return ok ? 0 : 1;
}

static void seed_descriptor_codes(uint8_t *params, unsigned group,
				  const uint8_t *primary,
				  const uint8_t *secondary, unsigned count)
{
	put_dword(params, 0x604 + group * 4U, count);
	memcpy(params + 0x004 + group * 0x80U, primary, count);
	memcpy(params + 0x304 + group * 0x80U, secondary, count);
}

static int run_mapped_params_case(const char *name, unsigned repeat,
				  unsigned info_source)
{
	static const uint8_t p0[] = { 0x00, 0x11, 0x22 };
	static const uint8_t s0[] = { 0x30, 0x41, 0x52 };
	static const uint8_t p2[] = { 0x00, 0x11, 0x22 };
	static const uint8_t s2[] = { 0x30, 0x41, 0x53 };
	static const uint8_t p3[] = { 0x7f, 0x60 };
	static const uint8_t s3[] = { 0x70, 0x6f };
	uint8_t params[0x700];
	uint8_t request[0x20];
	uint8_t cp[V92CP_BYTES];
	struct sl_v92_cp_params source_params;
	struct sl_v92_cp_request source_request;
	struct sl_v92_cp_info info;
	struct sl_v92_cp_bits source;
	uint32_t len = 0;
	uint8_t *bits;
	uint32_t write_pos;
	uint32_t padded_len;
	int ok;

	memset(params, 0, sizeof(params));
	memset(request, 0, sizeof(request));
	memset(cp, 0, sizeof(cp));
	memset(&source_params, 0, sizeof(source_params));
	memset(&source_request, 0, sizeof(source_request));
	V92CP_ctor(cp);

	put_dword(params, 0x000, info_source);
	put_dword(params, 0x620, 3);
	put_dword(params, 0x624, 3);
	put_dword(params, 0x61c, 1);
	put_float(params, 0x628, -0.75f);
	put_float(params, 0x62c, 0.5f);
	put_float(params, 0x630, -0.25f);
	put_float(params, 0x634, 0.125f);
	seed_descriptor_codes(params, 0, p0, s0, sizeof(p0));
	seed_descriptor_codes(params, 1, p0, s0, sizeof(p0));
	seed_descriptor_codes(params, 2, p2, s2, sizeof(p2));
	seed_descriptor_codes(params, 3, p3, s3, sizeof(p3));
	seed_descriptor_codes(params, 4, p0, s0, sizeof(p0));
	seed_descriptor_codes(params, 5, p3, s3, sizeof(p3));

	put_dword(request, 0x0, 1);
	put_dword(request, 0x4, repeat);
	put_float(request, 0x8, 1.25f);
	put_dword(request, 0xc, 1);
	put_dword(request, 0x10, 0);

	source_params.info_source = info_source;
	source_params.session_flags = 3;
	source_params.scalar_flags = 3;
	source_params.secondary_enabled = 1;
	source_params.signed_scalar[0] = -0.75f;
	source_params.signed_scalar[1] = 0.5f;
	source_params.signed_scalar[2] = -0.25f;
	source_params.signed_scalar[3] = 0.125f;
	source_params.descriptors[0] = (struct sl_v92_cp_descriptor_input){ p0, s0, sizeof(p0) };
	source_params.descriptors[1] = (struct sl_v92_cp_descriptor_input){ p0, s0, sizeof(p0) };
	source_params.descriptors[2] = (struct sl_v92_cp_descriptor_input){ p2, s2, sizeof(p2) };
	source_params.descriptors[3] = (struct sl_v92_cp_descriptor_input){ p3, s3, sizeof(p3) };
	source_params.descriptors[4] = (struct sl_v92_cp_descriptor_input){ p0, s0, sizeof(p0) };
	source_params.descriptors[5] = (struct sl_v92_cp_descriptor_input){ p3, s3, sizeof(p3) };
	source_request.tail_flag = 1;
	source_request.repeat = repeat;
	source_request.section_tag = 1;
	source_request.pad_repeat = 1;
	source_request.scalar_magnitude = 1.25f;
	source_request.suv = 0;

	setV92CPpckFromParamsInfo(params, request, cp);
	cp[0x128] = 1;
	V92CP_infoToBits(cp);
	bits = V92CP_getBitVector(cp, &len);
	write_pos = dword_at(cp, 0x11c);
	padded_len = dword_at(cp, 0x90c);

	ok = sl_v92_cp_info_from_params(&info, &source_params,
					&source_request) == 0 &&
		sl_v92_cp_pack_recovered(&info, &source) == 0 &&
		source.bit_count == len &&
		source.write_pos == write_pos &&
		source.padded_count == padded_len &&
		memcmp(source.bits, bits, len) == 0;
	if (ok) {
		struct sl_v92_cp_info unpacked;
		struct sl_v92_cp_bits repacked;

		ok = sl_v92_cp_unpack_recovered(&unpacked, source.bits,
						source.write_pos) == 0 &&
			sl_v92_cp_pack_recovered(&unpacked, &repacked) == 0 &&
			repacked.write_pos == source.write_pos &&
			memcmp(repacked.bits, source.bits,
			       source.write_pos) == 0 &&
			compare_source_stream_decode(name, &info, &source) == 0 &&
			compare_unpacked_params(name, &info, cp) == 0 &&
			compare_blob_decode(name, bits, write_pos, cp) == 0;
	}
	printf("%s len=%u write=%u padded=%u "
	       "unique=%u map=%u,%u,%u,%u,%u,%u result=%s\n",
	       name, len, write_pos, padded_len, info.descriptor_count,
	       info.short_scalar_a[0], info.short_scalar_a[1],
	       info.short_scalar_a[2], info.short_scalar_a[3],
	       info.short_scalar_b[0], info.short_scalar_b[1],
	       ok ? "ok" : "FAIL");
	if (!ok) {
		dump_bits(name, bits, len);
		if (source.bit_count)
			dump_bits("v92cp-mapped-params-source", source.bits,
				  source.bit_count);
	}
	return ok ? 0 : 1;
}

static void prepare_default_suv(uint8_t *cp)
{
	cp[0] = 1;
	cp[4] = 0;
	cp[0x128] = 1;
	V92CP_setSUV(cp, 1);
}

static void prepare_state1_manual(uint8_t *cp)
{
	cp[0] = 1;
	cp[4] = 1;
	cp[0x128] = 1;
	put_word(cp, 0x104, 3);
	put_dword(cp, 0x108, 1);
}

static void prepare_state1_pad2(uint8_t *cp)
{
	prepare_state1_manual(cp);
	cp[0x128] = 2;
}

static void prepare_short_session(uint8_t *cp)
{
	cp[0] = 0;
	cp[1] = 0;
	cp[2] = 0;
	cp[4] = 0;
	cp[0x128] = 1;
	put_dword(cp, 0x8, 1);
}

static void prepare_two_section(uint8_t *cp)
{
	cp[0] = 0;
	cp[1] = 2;
	cp[2] = 0x15;
	cp[3] = 0x01;
	cp[4] = 1;
	cp[0x128] = 1;
	put_dword(cp, 0x8, 3);
}

static void prepare_short_flags(uint8_t *cp)
{
	cp[0] = 0;
	cp[1] = 0;
	cp[2] = 0x15;
	cp[3] = 0x01;
	cp[4] = 1;
	cp[0x128] = 1;
	put_dword(cp, 0x8, 3);
}

static void prepare_short_flags_pad2(uint8_t *cp)
{
	prepare_short_flags(cp);
	cp[0x128] = 2;
}

static void prepare_primary_descriptor(uint8_t *cp)
{
	static const uint8_t codes[] = {
		0x00, 0x1f,
		0x24, 0x25, 0x26, 0x27,
		0x32, 0x34, 0x35, 0x39, 0x3c,
		0x41, 0x4e,
		0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5b,
		0x61, 0x63, 0x65, 0x67, 0x69, 0x6b, 0x6d, 0x6f,
		0x70, 0x72, 0x74, 0x76, 0x78, 0x7a, 0x7c, 0x7e,
	};
	uint16_t words[SL_V92_CP_WORDS_PER_DESCRIPTOR];
	unsigned i;

	prepare_short_flags(cp);
	put_word(cp, 0x10c, 1);
	if (sl_v92_cp_build_descriptor(words, codes,
				       sizeof(codes) / sizeof(codes[0])) != 0)
		return;
	for (i = 0; i < SL_V92_CP_WORDS_PER_DESCRIPTOR; i++)
		put_word(cp, 0x42 + i * 2U, words[i]);
}

static void prepare_scalar_block(uint8_t *cp)
{
	prepare_primary_descriptor(cp);
	put_dword(cp, 0xc, 3);
	put_float(cp, 0x10, 1.25f);
	put_float(cp, 0x14, -0.75f);
	put_float(cp, 0x18, 0.5f);
	put_float(cp, 0x1c, -0.25f);
	put_float(cp, 0x20, 0.125f);
	put_word(cp, 0x28, 0x0005);
	put_word(cp, 0x2c, 0x000a);
	put_word(cp, 0x30, 0x0003);
	put_word(cp, 0x34, 0x000c);
	put_word(cp, 0x38, 0x0006);
	put_word(cp, 0x3c, 0x0009);
}

static void prepare_secondary_descriptor(uint8_t *cp)
{
	static const uint8_t codes[] = {
		0x00, 0x08, 0x11, 0x19, 0x22, 0x2a, 0x33, 0x3b,
		0x40, 0x44, 0x48, 0x4c, 0x51, 0x55, 0x59, 0x5d,
		0x62, 0x66, 0x6a, 0x6e, 0x73, 0x77, 0x7b, 0x7f,
	};
	uint16_t secondary[SL_V92_CP_WORDS_PER_DESCRIPTOR];
	unsigned i;

	prepare_primary_descriptor(cp);
	cp[0x24] = 1;
	if (sl_v92_cp_build_descriptor(secondary, codes,
				       sizeof(codes) / sizeof(codes[0])) != 0)
		return;
	for (i = 0; i < SL_V92_CP_WORDS_PER_DESCRIPTOR; i++)
		put_word(cp, 0xa2 + i * 2U, secondary[i]);
}

static int run_descriptor_builder_selftest(void)
{
	static const uint8_t codes[] = { 0x00, 0x00, 0x0f, 0x10, 0x71, 0x7f };
	static const uint8_t expected_inverse[] = { 0x7f, 0x71, 0x10, 0x0f, 0x00 };
	uint16_t descriptor[SL_V92_CP_WORDS_PER_DESCRIPTOR];
	uint8_t inverse[8];
	unsigned inverse_count = 0;
	int ok;

	ok = sl_v92_cp_build_descriptor(descriptor, codes,
					sizeof(codes) / sizeof(codes[0])) == 0 &&
		descriptor[0] == 0x8001U &&
		descriptor[1] == 0x0001U &&
		descriptor[7] == 0x8002U;
	printf("v92cp-descriptor-builder result=%s\n", ok ? "ok" : "FAIL");
	if (!ok)
		return 1;
	ok = sl_v92_cp_descriptor_to_codes(descriptor, inverse, sizeof(inverse),
					   &inverse_count) == 0 &&
		inverse_count == sizeof(expected_inverse) &&
		memcmp(inverse, expected_inverse, sizeof(expected_inverse)) == 0;
	printf("v92cp-descriptor-inverse result=%s\n", ok ? "ok" : "FAIL");
	if (!ok)
		return 1;
	ok = sl_v92_cp_descriptor_to_codes(descriptor, inverse, 1U,
					   &inverse_count) == -ENOSPC &&
		inverse_count == 1U;
	printf("v92cp-descriptor-inverse-short result=%s\n",
	       ok ? "ok" : "FAIL");
	if (!ok)
		return 1;
	ok = sl_v92_cp_build_descriptor(descriptor, (const uint8_t *)"\x80",
					1U) == -EINVAL;
	printf("v92cp-descriptor-builder-invalid result=%s\n",
	       ok ? "ok" : "FAIL");
	return ok ? 0 : 1;
}

static int run_descriptor_plan_selftest(void)
{
	static const uint8_t p0[] = { 0x00, 0x11, 0x22 };
	static const uint8_t s0[] = { 0x30, 0x41, 0x52 };
	static const uint8_t p2[] = { 0x00, 0x11, 0x22 };
	static const uint8_t s2[] = { 0x30, 0x41, 0x53 };
	static const uint8_t p3[] = { 0x7f };
	static const uint8_t s3[] = { 0x70 };
	static const struct sl_v92_cp_descriptor_input groups[] = {
		{ p0, s0, sizeof(p0) },
		{ p0, s0, sizeof(p0) },
		{ p2, s2, sizeof(p2) },
		{ p3, s3, sizeof(p3) },
	};
	struct sl_v92_cp_descriptor_plan plan;
	struct sl_v92_cp_info info;
	int ok;

	memset(&info, 0, sizeof(info));
	ok = sl_v92_cp_build_descriptor_plan(
		     &plan, groups, sizeof(groups) / sizeof(groups[0])) == 0 &&
		sl_v92_cp_apply_descriptor_plan(&info, &plan, 1U) == 0 &&
		plan.unique_count == 3U &&
		info.descriptor_count == 3U &&
		info.secondary_enabled == 1U &&
		plan.source_to_unique[0] == 0U &&
		plan.source_to_unique[1] == 0U &&
		plan.source_to_unique[2] == 1U &&
		plan.source_to_unique[3] == 2U &&
		plan.primary_descriptor[0][0] == 0x0001U &&
		plan.primary_descriptor[0][1] == 0x0002U &&
		plan.primary_descriptor[0][2] == 0x0004U &&
		plan.secondary_descriptor[1][5] == 0x0008U &&
		plan.primary_descriptor[2][7] == 0x8000U &&
		info.primary_descriptor[2][7] == 0x8000U;
	printf("v92cp-descriptor-plan result=%s\n", ok ? "ok" : "FAIL");
	return ok ? 0 : 1;
}

int main(void)
{
	int ret = 0;

	ret |= run_descriptor_builder_selftest();
	ret |= run_descriptor_plan_selftest();
	ret |= run_mapped_params_case("v92cp-mapped-params", 0U, 0x1dU);
	ret |= run_mapped_params_case("v92cp-mapped-params-repeat1", 1U, 0x25U);
	ret |= run_case("v92cp-default-suv", prepare_default_suv);
	ret |= run_case("v92cp-state1-manual", prepare_state1_manual);
	ret |= run_case("v92cp-state1-pad2", prepare_state1_pad2);
	ret |= run_case("v92cp-short-session", prepare_short_session);
	ret |= run_case("v92cp-short-flags", prepare_short_flags);
	ret |= run_case("v92cp-short-flags-pad2", prepare_short_flags_pad2);
	ret |= run_case("v92cp-primary-descriptor", prepare_primary_descriptor);
	ret |= run_case("v92cp-scalar-block", prepare_scalar_block);
	ret |= run_case("v92cp-secondary-descriptor", prepare_secondary_descriptor);
	ret |= run_case("v92cp-two-section", prepare_two_section);
	return ret ? 1 : 0;
}
