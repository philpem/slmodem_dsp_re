/*
 * Recovered subset of SmartLink V92CP control-packet handling.
 *
 * Source evidence:
 *   - dsplibs.o:.text 0x4ec80..0x4f3d0
 *   - re/v92_cp_probe blob outputs
 *
 * The transmit side is a variable control-packet packer with several session
 * layouts. The state-1 compact path and state-0 short-session layouts below
 * are packed field-by-field and use the recovered CRC/padding tail from the
 * original object. The receive side includes the matching normalized unpacker
 * and the recovered length-inference logic from V92CP::bitsToInfo(). The
 * descriptor tables at offsets 0x42/0xa2 are encoded as groups of eight
 * 16-bit LSB-first masks, each preceded by a delimiter bit. The upstream mask
 * population uses byte codes where the high nibble selects the mask word and
 * the low nibble selects the bit inside that word; the six descriptor groups
 * are deduplicated by matching primary/secondary code arrays. The reverse
 * parameter mapper uses SmartLink's recovered mask-to-byte traversal: mask
 * words are visited high-to-low, but bits are shifted low-to-high while the
 * emitted descriptor bit label counts 15 down to 0. Remaining work is mostly
 * around the full SmartLink resynchronizing receive detector and any unobserved
 * packet layouts outside the recovered state-0/state-1 paths.
 */

#include <errno.h>
#include <string.h>

#include "sl_v92_cp.h"

void sl_v92_cp_clear_descriptor(uint16_t descriptor
				[SL_V92_CP_WORDS_PER_DESCRIPTOR])
{
	memset(descriptor, 0, SL_V92_CP_WORDS_PER_DESCRIPTOR *
	       sizeof(descriptor[0]));
}

int sl_v92_cp_build_descriptor(uint16_t descriptor
			       [SL_V92_CP_WORDS_PER_DESCRIPTOR],
			       const uint8_t *codes, unsigned count)
{
	unsigned i;

	if (!descriptor || (!codes && count))
		return -EINVAL;
	sl_v92_cp_clear_descriptor(descriptor);
	for (i = 0; i < count; i++) {
		unsigned word = codes[i] >> 4U;
		unsigned bit = codes[i] & 0x0fU;

		if (word >= SL_V92_CP_WORDS_PER_DESCRIPTOR)
			return -EINVAL;
		descriptor[word] |= (uint16_t)(1U << bit);
	}
	return 0;
}

int sl_v92_cp_descriptor_to_codes(const uint16_t descriptor
				  [SL_V92_CP_WORDS_PER_DESCRIPTOR],
				  uint8_t *codes, unsigned capacity,
				  unsigned *count)
{
	unsigned out = 0;
	int word;

	if (!descriptor || !count)
		return -EINVAL;
	for (word = (int)SL_V92_CP_WORDS_PER_DESCRIPTOR - 1; word >= 0; word--) {
		int bit;

		for (bit = 15; bit >= 0; bit--) {
			if (!(descriptor[word] & (uint16_t)(1U << bit)))
				continue;
			if (out >= capacity) {
				*count = out;
				return -ENOSPC;
			}
			if (!codes)
				return -EINVAL;
			codes[out++] = (uint8_t)(((unsigned)word << 4U) |
						 (unsigned)bit);
		}
	}
	*count = out;
	return 0;
}

static int descriptor_to_smartlink_param_codes(
	const uint16_t descriptor[SL_V92_CP_WORDS_PER_DESCRIPTOR],
	uint8_t *codes, unsigned capacity, unsigned *count)
{
	unsigned out = 0;
	int word;

	if (!descriptor || !codes || !count)
		return -EINVAL;
	for (word = (int)SL_V92_CP_WORDS_PER_DESCRIPTOR - 1; word >= 0; word--) {
		unsigned bit;

		for (bit = 0; bit < 16U; bit++) {
			if (!(descriptor[word] & (uint16_t)(1U << bit)))
				continue;
			if (out >= capacity) {
				*count = out;
				return -ENOSPC;
			}
			codes[out++] = (uint8_t)(((unsigned)word << 4U) |
						 (15U - bit));
		}
	}
	*count = out;
	return 0;
}


static int descriptor_groups_equal(const struct sl_v92_cp_descriptor_input *a,
				   const struct sl_v92_cp_descriptor_input *b)
{
	if (a->code_count != b->code_count)
		return 0;
	if (!a->code_count)
		return 1;
	if (!a->primary_codes || !b->primary_codes ||
	    !a->secondary_codes || !b->secondary_codes)
		return 0;
	return memcmp(a->primary_codes, b->primary_codes, a->code_count) == 0 &&
		memcmp(a->secondary_codes, b->secondary_codes,
		       a->code_count) == 0;
}

int sl_v92_cp_build_descriptor_plan(struct sl_v92_cp_descriptor_plan *plan,
				    const struct sl_v92_cp_descriptor_input *groups,
				    unsigned group_count)
{
	unsigned group;

	if (!plan || (!groups && group_count) ||
	    group_count > SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
		return -EINVAL;
	memset(plan, 0, sizeof(*plan));

	for (group = 0; group < group_count; group++) {
		unsigned unique;

		for (unique = 0; unique < plan->unique_count; unique++) {
			unsigned representative = 0;
			unsigned i;

			for (i = 0; i < group; i++) {
				if (plan->source_to_unique[i] == unique) {
					representative = i;
					break;
				}
			}
			if (i < group &&
			    descriptor_groups_equal(&groups[group],
						    &groups[representative]))
				break;
		}
		if (unique == plan->unique_count) {
			int rc;

			if (plan->unique_count >= SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
				return -EINVAL;
			rc = sl_v92_cp_build_descriptor(
				plan->primary_descriptor[unique],
				groups[group].primary_codes,
				groups[group].code_count);
			if (rc != 0)
				return rc;
			rc = sl_v92_cp_build_descriptor(
				plan->secondary_descriptor[unique],
				groups[group].secondary_codes,
				groups[group].code_count);
			if (rc != 0)
				return rc;
			plan->unique_count++;
		}
		plan->source_to_unique[group] = unique;
	}
	return 0;
}

int sl_v92_cp_apply_descriptor_plan(struct sl_v92_cp_info *info,
				    const struct sl_v92_cp_descriptor_plan *plan,
				    unsigned secondary_enabled)
{
	if (!info || !plan ||
	    plan->unique_count > SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
		return -EINVAL;
	info->descriptor_count = plan->unique_count;
	info->secondary_enabled = secondary_enabled ? 1U : 0U;
	memcpy(info->primary_descriptor, plan->primary_descriptor,
	       sizeof(info->primary_descriptor));
	memcpy(info->secondary_descriptor, plan->secondary_descriptor,
	       sizeof(info->secondary_descriptor));
	return 0;
}

int sl_v92_cp_info_from_params(struct sl_v92_cp_info *info,
			       const struct sl_v92_cp_params *params,
			       const struct sl_v92_cp_request *request)
{
	struct sl_v92_cp_descriptor_plan plan;
	unsigned i;
	int rc;

	if (!info || !params || !request)
		return -EINVAL;
	memset(info, 0, sizeof(*info));

	rc = sl_v92_cp_build_descriptor_plan(&plan, params->descriptors,
					     SL_V92_CP_MAX_DESCRIPTOR_GROUPS);
	if (rc != 0)
		return rc;
	rc = sl_v92_cp_apply_descriptor_plan(info, &plan,
					     params->secondary_enabled);
	if (rc != 0)
		return rc;

	info->state = 0;
	info->repeat = request->repeat;
	info->tail_flag = request->tail_flag;
	info->section_tag = request->section_tag;
	info->pad_repeat = request->pad_repeat;
	info->scalar_magnitude = request->scalar_magnitude;
	info->suv = request->suv;
	info->session_flags = params->session_flags;
	info->scalar_flags = params->scalar_flags;
	if (params->info_source < (request->repeat ? 20U : 8U))
		return -EINVAL;
	info->info_byte = params->info_source - (request->repeat ? 20U : 8U);
	for (i = 0; i < 4U; i++) {
		info->signed_scalar[i] = params->signed_scalar[i];
		info->short_scalar_a[i] = (uint16_t)plan.source_to_unique[i];
	}
	for (i = 0; i < 2U; i++)
		info->short_scalar_b[i] =
			(uint16_t)plan.source_to_unique[i + 4U];
	return 0;
}

int sl_v92_cp_params_from_info(struct sl_v92_cp_unpacked_params *params,
			       const struct sl_v92_cp_info *info)
{
	unsigned i;

	if (!params || !info)
		return -EINVAL;
	if (info->descriptor_count > SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
		return -EINVAL;
	memset(params, 0, sizeof(*params));

	params->info_source = info->info_byte + (info->repeat ? 20U : 8U);
	params->session_flags = info->session_flags;
	params->scalar_flags = info->scalar_flags;
	params->secondary_enabled = info->secondary_enabled ? 1U : 0U;
	for (i = 0; i < 4U; i++)
		params->signed_scalar[i] = info->signed_scalar[i];
	for (i = 0; i < 4U; i++)
		params->source_to_unique[i] = info->short_scalar_a[i];
	for (i = 0; i < 2U; i++)
		params->source_to_unique[i + 4U] = info->short_scalar_b[i];

	for (i = 0; i < SL_V92_CP_MAX_DESCRIPTOR_GROUPS; i++) {
		unsigned unique = params->source_to_unique[i];
		unsigned primary_count = 0;
		unsigned secondary_count = 0;
		const uint16_t (*secondary_table)[SL_V92_CP_WORDS_PER_DESCRIPTOR];
		int rc;

		if (unique >= info->descriptor_count)
			return -EINVAL;
		rc = descriptor_to_smartlink_param_codes(
			info->primary_descriptor[unique],
			params->primary_codes[i],
			sizeof(params->primary_codes[i]), &primary_count);
		if (rc != 0)
			return rc;
		secondary_table = info->secondary_enabled ?
			info->secondary_descriptor : info->primary_descriptor;
		rc = descriptor_to_smartlink_param_codes(
			secondary_table[unique],
			params->secondary_codes[i],
			sizeof(params->secondary_codes[i]), &secondary_count);
		if (rc != 0)
			return rc;
		if (secondary_count != primary_count)
			return -EINVAL;
		params->code_count[i] = primary_count;
	}
	return 0;
}

static void put_lsb_bits(uint8_t *bits, unsigned *pos, unsigned value,
			 unsigned count)
{
	unsigned i;

	for (i = 0; i < count; i++)
		bits[(*pos)++] = (uint8_t)((value >> i) & 1U);
}

static void append_descriptor_table(uint8_t *bits, unsigned *pos,
				    const uint16_t table
				    [SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				    [SL_V92_CP_WORDS_PER_DESCRIPTOR],
				    unsigned count)
{
	unsigned group;
	unsigned word;

	for (group = 0; group < count; group++) {
		for (word = 0; word < SL_V92_CP_WORDS_PER_DESCRIPTOR; word++) {
			bits[(*pos)++] = 0;
			put_lsb_bits(bits, pos, table[group][word], 16U);
		}
	}
}

static unsigned get_lsb_bits(const uint8_t *bits, unsigned pos,
			     unsigned count)
{
	unsigned value = 0;
	unsigned i;

	for (i = 0; i < count; i++)
		value |= (unsigned)(bits[pos + i] & 1U) << i;
	return value;
}

static unsigned inferred_descriptor_count_from_bits(const uint8_t *bits)
{
	unsigned max_index = 0;
	unsigned i;

	for (i = 0; i < 4U; i++) {
		unsigned index = get_lsb_bits(bits, 103U + i * 4U, 4U);

		if (index > max_index)
			max_index = index;
	}
	for (i = 0; i < 2U; i++) {
		unsigned index = get_lsb_bits(bits, 120U + i * 4U, 4U);

		if (index > max_index)
			max_index = index;
	}
	return max_index + 1U;
}

static void read_descriptor_table(const uint8_t *bits, unsigned *pos,
				  uint16_t table
				  [SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				  [SL_V92_CP_WORDS_PER_DESCRIPTOR],
				  unsigned count)
{
	unsigned group;
	unsigned word;

	for (group = 0; group < count; group++) {
		for (word = 0; word < SL_V92_CP_WORDS_PER_DESCRIPTOR; word++) {
			(*pos)++;
			table[group][word] =
				(uint16_t)get_lsb_bits(bits, *pos, 16U);
			*pos += 16U;
		}
	}
}

static float abs_float(float value)
{
	return value < 0.0f ? -value : value;
}

static void put_quantized_bits_reverse(uint8_t *bits, unsigned high_pos,
				       float value, const float *table,
				       unsigned count)
{
	float remaining = abs_float(value);
	unsigned i;

	for (i = 0; i < count; i++) {
		if (table[i] <= remaining) {
			bits[high_pos - i] = 1;
			remaining -= table[i];
		} else {
			bits[high_pos - i] = 0;
		}
	}
}

static void pack_state0_scalar_block(const struct sl_v92_cp_info *info,
				     struct sl_v92_cp_bits *out)
{
	static const float table2[16] = {
		4.0f, 2.0f, 1.0f, 0.5f,
		0.25f, 0.125f, 0.0625f, 0.03125f,
		0.015625f, 0.0078125f, 0.00390625f, 0.001953125f,
		0.0009765625f, 0.00048828125f, 0.000244140625f,
		0.0001220703125f,
	};
	static const float table1[7] = {
		1.0f, 0.5f, 0.25f, 0.125f,
		0.0625f, 0.03125f, 0.015625f,
	};
	unsigned i;

	out->bits[49] = (uint8_t)(info->scalar_flags & 1U);
	out->bits[50] = (uint8_t)((info->scalar_flags >> 1U) & 1U);
	out->bits[51] = 0;
	put_quantized_bits_reverse(out->bits, 67U, info->scalar_magnitude,
				   table2, 16U);
	out->bits[68] = 0;
	put_quantized_bits_reverse(out->bits, 75U, info->signed_scalar[0],
				   table1, 7U);
	out->bits[76] = info->signed_scalar[0] < 0.0f;
	put_quantized_bits_reverse(out->bits, 83U, info->signed_scalar[1],
				   table1, 7U);
	out->bits[84] = info->signed_scalar[1] < 0.0f;
	out->bits[85] = 0;
	put_quantized_bits_reverse(out->bits, 92U, info->signed_scalar[2],
				   table1, 7U);
	out->bits[93] = info->signed_scalar[2] < 0.0f;
	put_quantized_bits_reverse(out->bits, 100U, info->signed_scalar[3],
				   table1, 7U);
	out->bits[101] = info->signed_scalar[3] < 0.0f;
	out->bits[102] = 0;
	for (i = 0; i < 4U; i++) {
		unsigned pos = 103U + i * 4U;

		put_lsb_bits(out->bits, &pos, info->short_scalar_a[i], 4U);
	}
	out->bits[119] = 0;
	for (i = 0; i < 2U; i++) {
		unsigned pos = 120U + i * 4U;

		put_lsb_bits(out->bits, &pos, info->short_scalar_b[i], 4U);
	}
}

static float read_quantized_bits_reverse(const uint8_t *bits,
					 unsigned high_pos,
					 const float *table,
					 unsigned count)
{
	float value = 0.0f;
	unsigned i;

	for (i = 0; i < count; i++) {
		if (bits[high_pos - i])
			value += table[i];
	}
	return value;
}

static void unpack_state0_scalar_block(struct sl_v92_cp_info *info,
				       const uint8_t *bits)
{
	static const float table2[16] = {
		4.0f, 2.0f, 1.0f, 0.5f,
		0.25f, 0.125f, 0.0625f, 0.03125f,
		0.015625f, 0.0078125f, 0.00390625f, 0.001953125f,
		0.0009765625f, 0.00048828125f, 0.000244140625f,
		0.0001220703125f,
	};
	static const float table1[7] = {
		1.0f, 0.5f, 0.25f, 0.125f,
		0.0625f, 0.03125f, 0.015625f,
	};
	unsigned i;

	info->scalar_flags = (bits[49] & 1U) | ((bits[50] & 1U) << 1U);
	info->scalar_magnitude = read_quantized_bits_reverse(bits, 67U,
							     table2, 16U);
	info->signed_scalar[0] = read_quantized_bits_reverse(bits, 75U,
							     table1, 7U);
	if (bits[76])
		info->signed_scalar[0] = -info->signed_scalar[0];
	info->signed_scalar[1] = read_quantized_bits_reverse(bits, 83U,
							     table1, 7U);
	if (bits[84])
		info->signed_scalar[1] = -info->signed_scalar[1];
	info->signed_scalar[2] = read_quantized_bits_reverse(bits, 92U,
							     table1, 7U);
	if (bits[93])
		info->signed_scalar[2] = -info->signed_scalar[2];
	info->signed_scalar[3] = read_quantized_bits_reverse(bits, 100U,
							     table1, 7U);
	if (bits[101])
		info->signed_scalar[3] = -info->signed_scalar[3];
	for (i = 0; i < 4U; i++)
		info->short_scalar_a[i] =
			(uint16_t)get_lsb_bits(bits, 103U + i * 4U, 4U);
	for (i = 0; i < 2U; i++)
		info->short_scalar_b[i] =
			(uint16_t)get_lsb_bits(bits, 120U + i * 4U, 4U);
	info->secondary_enabled = bits[128] & 1U;
}

static void v92_cp_append_tail(struct sl_v92_cp_bits *out, unsigned write_pos,
			       unsigned repeat_count)
{
	uint8_t crc[16];
	unsigned i;
	unsigned pos;
	unsigned pad_unit = repeat_count * 12U;

	out->bits[write_pos] = 0;
	for (i = 0; i < 16U; i++)
		crc[i] = 1;

	/*
	 * SmartLink starts the CP parity scan after the 17-bit all-ones
	 * preamble and skips every 17th delimiter position. The register shifts
	 * toward bit zero; feedback is applied at output positions 3 and 10.
	 * This matches the captured state-0 156-bit and compact 60-bit layouts.
	 */
	i = 17U;
	while (i < write_pos) {
		uint8_t feedback;
		uint8_t next[16];
		unsigned source = i;

		if ((source % 17U) == 0)
			source++;
		feedback = (uint8_t)((crc[0] + out->bits[source]) & 1U);
		next[0] = crc[1];
		next[1] = crc[2];
		next[2] = crc[3];
		next[3] = (uint8_t)((crc[4] + feedback) & 1U);
		next[4] = crc[5];
		next[5] = crc[6];
		next[6] = crc[7];
		next[7] = crc[8];
		next[8] = crc[9];
		next[9] = crc[10];
		next[10] = (uint8_t)((crc[11] + feedback) & 1U);
		next[11] = crc[12];
		next[12] = crc[13];
		next[13] = crc[14];
		next[14] = crc[15];
		next[15] = feedback;
		memcpy(crc, next, sizeof(crc));
		i = source + 1U;
	}

	pos = write_pos + 1U;
	for (i = 0; i < 16U; i++)
		out->bits[pos++] = crc[i];
	out->bits[pos++] = 0;
	out->write_pos = pos;

	if (!pad_unit)
		pad_unit = 12U;
	out->padded_count = ((pos / pad_unit) + 1U) * pad_unit;
	out->bit_count = out->padded_count;
}

static void v92_cp_compute_crc(const uint8_t *bits, unsigned write_pos,
			       uint8_t crc[16])
{
	unsigned i;

	for (i = 0; i < 16U; i++)
		crc[i] = 1;

	i = 17U;
	while (i < write_pos) {
		uint8_t feedback;
		uint8_t next[16];
		unsigned source = i;

		if ((source % 17U) == 0)
			source++;
		feedback = (uint8_t)((crc[0] + bits[source]) & 1U);
		next[0] = crc[1];
		next[1] = crc[2];
		next[2] = crc[3];
		next[3] = (uint8_t)((crc[4] + feedback) & 1U);
		next[4] = crc[5];
		next[5] = crc[6];
		next[6] = crc[7];
		next[7] = crc[8];
		next[8] = crc[9];
		next[9] = crc[10];
		next[10] = (uint8_t)((crc[11] + feedback) & 1U);
		next[11] = crc[12];
		next[12] = crc[13];
		next[13] = crc[14];
		next[14] = crc[15];
		next[15] = feedback;
		memcpy(crc, next, sizeof(next));
		i = source + 1U;
	}
}

static int v92_cp_check_tail(const uint8_t *bits, unsigned total_write_pos)
{
	uint8_t crc[16];
	unsigned i;
	unsigned tail_start;

	if (total_write_pos < 18U)
		return 0;
	tail_start = total_write_pos - 18U;
	if (bits[tail_start])
		return 0;
	v92_cp_compute_crc(bits, tail_start, crc);
	for (i = 0; i < 16U; i++) {
		if (bits[tail_start + 1U + i] != crc[i])
			return 0;
	}
	return bits[tail_start + 17U] == 0;
}

static int pack_state0_recovered(const struct sl_v92_cp_info *info,
				 struct sl_v92_cp_bits *out)
{
	unsigned pos = 0;
	unsigned i;
	unsigned descriptor_count = info->descriptor_count;
	unsigned pad_repeat = info->pad_repeat;

	if (descriptor_count > SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
		return -EINVAL;
	if (!pad_repeat)
		pad_repeat = 1U;

	for (i = 0; i < 17U; i++)
		out->bits[pos++] = 1;
	out->bits[pos++] = 0;
	out->bits[pos++] = 0;
	put_lsb_bits(out->bits, &pos, info->repeat, 2U);
	put_lsb_bits(out->bits, &pos, info->info_byte, 5U);
	for (i = 0; i < 7U; i++)
		out->bits[pos++] = 0;
	if (info->repeat <= 1U) {
		out->bits[31] = (uint8_t)(info->session_flags & 1U);
		out->bits[32] = (uint8_t)((info->session_flags >> 1) & 1U);
		out->bits[pos++] = (uint8_t)(info->tail_flag & 1U);
		out->bits[pos++] = 0;
		out->bits[pos++] = (uint8_t)(info->section_tag & 1U);
		put_lsb_bits(out->bits, &pos, info->info_byte >> 5U, 13U);
		while (pos < 136U)
			out->bits[pos++] = 0;
		pack_state0_scalar_block(info, out);
		out->bits[128] = (uint8_t)(info->secondary_enabled & 1U);
		append_descriptor_table(out->bits, &pos, info->primary_descriptor,
					descriptor_count);
		if (info->secondary_enabled)
			append_descriptor_table(out->bits, &pos,
						info->secondary_descriptor,
						descriptor_count);
		v92_cp_append_tail(out, pos, pad_repeat);
		return 0;
	}

	out->bits[pos++] = (uint8_t)(info->tail_flag & 1U);
	v92_cp_append_tail(out, pos, pad_repeat);
	return 0;
}

static int pack_state1_recovered(const struct sl_v92_cp_info *info,
				 struct sl_v92_cp_bits *out)
{
	unsigned pos = 0;
	unsigned i;
	unsigned pad_repeat = info->pad_repeat;

	if (!pad_repeat)
		pad_repeat = 1U;

	for (i = 0; i < 17U; i++)
		out->bits[pos++] = 1;
	out->bits[pos++] = 0;
	out->bits[pos++] = 1;
	while (pos < 27U)
		out->bits[pos++] = 0;
	put_lsb_bits(out->bits, &pos, info->state1_code, 5U);
	out->bits[pos++] = (uint8_t)(info->suv & 1U);
	out->bits[pos++] = (uint8_t)(info->tail_flag & 1U);
	v92_cp_append_tail(out, pos, pad_repeat);
	return 0;
}

int sl_v92_cp_pack_recovered(const struct sl_v92_cp_info *info,
			     struct sl_v92_cp_bits *out)
{
	if (!info || !out)
		return -EINVAL;
	memset(out, 0, sizeof(*out));

	if (info->state == 1U)
		return pack_state1_recovered(info, out);

	if (info->state == 0U && info->suv == 0U)
		return pack_state0_recovered(info, out);

	return -ENOTSUP;
}

int sl_v92_cp_unpack_recovered(struct sl_v92_cp_info *info,
			       const uint8_t *bits, unsigned bit_count)
{
	unsigned i;
	unsigned pos;

	if (!info || !bits)
		return -EINVAL;
	if (bit_count < 52U)
		return -EINVAL;
	memset(info, 0, sizeof(*info));
	/*
	 * This source-level unpacker returns normalized descriptor masks: the
	 * same representation accepted by sl_v92_cp_pack_recovered(). The blob's
	 * streaming bitsToInfo() stores each decoded 16-bit descriptor word
	 * bit-reversed internally, then compensates when converting masks back
	 * to descriptor byte codes.
	 */
	for (i = 0; i < 17U; i++) {
		if (!bits[i])
			return -EINVAL;
	}
	if (bits[17])
		return -EINVAL;

	info->state = bits[18] & 1U;
	if (info->state == 1U) {
		if (bit_count < 52U)
			return -EINVAL;
		info->state1_code = get_lsb_bits(bits, 27U, 5U);
		info->suv = bits[32] & 1U;
		info->tail_flag = bits[33] & 1U;
		return 0;
	}

	info->repeat = get_lsb_bits(bits, 19U, 2U);
	info->info_byte = get_lsb_bits(bits, 21U, 5U);
	if (info->repeat > 1U) {
		info->tail_flag = bits[33] & 1U;
		return 0;
	}

	if (bit_count < 154U)
		return -EINVAL;
	info->session_flags = (bits[31] & 1U) | ((bits[32] & 1U) << 1U);
	info->tail_flag = bits[33] & 1U;
	info->section_tag = bits[35] & 1U;
	info->info_byte |= get_lsb_bits(bits, 36U, 13U) << 5U;
	unpack_state0_scalar_block(info, bits);

	pos = 136U;
	if (bit_count > 154U) {
		unsigned descriptor_bits = bit_count - 154U;
		unsigned per_table = SL_V92_CP_WORDS_PER_DESCRIPTOR * 17U;
		unsigned tables = info->secondary_enabled ? 2U : 1U;

		if ((descriptor_bits % (per_table * tables)) != 0)
			return -EINVAL;
		info->descriptor_count =
			descriptor_bits / (per_table * tables);
		if (info->descriptor_count > SL_V92_CP_MAX_DESCRIPTOR_GROUPS)
			return -EINVAL;
		read_descriptor_table(bits, &pos, info->primary_descriptor,
				      info->descriptor_count);
		if (info->secondary_enabled)
			read_descriptor_table(bits, &pos,
					      info->secondary_descriptor,
					      info->descriptor_count);
	}
	return 0;
}

void sl_v92_cp_decoder_reset(struct sl_v92_cp_decoder *decoder,
			     unsigned pad_repeat)
{
	if (!decoder)
		return;
	memset(decoder, 0, sizeof(*decoder));
	decoder->pad_repeat = pad_repeat ? pad_repeat : 1U;
}

static unsigned decoder_expected_write_pos(const struct sl_v92_cp_decoder *d)
{
	const uint8_t *bits = d->bits;
	unsigned descriptor_count;
	unsigned secondary_enabled;

	if (d->bit_count < 19U)
		return 0;
	if (bits[18])
		return 52U;
	if (d->bit_count < 34U)
		return 0;
	if (get_lsb_bits(bits, 19U, 2U) > 1U)
		return 52U;
	if (d->bit_count < 136U)
		return 0;
	descriptor_count = inferred_descriptor_count_from_bits(bits);
	secondary_enabled = bits[128] & 1U;
	return 154U + descriptor_count * SL_V92_CP_WORDS_PER_DESCRIPTOR *
		17U * (secondary_enabled ? 2U : 1U);
}

int sl_v92_cp_decoder_feed(struct sl_v92_cp_decoder *decoder, uint8_t bit)
{
	unsigned expected_write_pos;

	if (!decoder)
		return SL_V92_CP_DECODE_ERROR;
	if (decoder->bit_count >= SL_V92_CP_MAX_BITS)
		return SL_V92_CP_DECODE_ERROR;
	decoder->bits[decoder->bit_count++] = (uint8_t)(bit & 1U);
	expected_write_pos = decoder_expected_write_pos(decoder);
	if (expected_write_pos > SL_V92_CP_MAX_BITS)
		return SL_V92_CP_DECODE_ERROR;
	decoder->expected_write_pos = expected_write_pos;
	if (!expected_write_pos || decoder->bit_count < expected_write_pos)
		return SL_V92_CP_DECODE_PENDING;
	if (decoder->bit_count < expected_write_pos)
		return SL_V92_CP_DECODE_PENDING;
	if (!v92_cp_check_tail(decoder->bits, expected_write_pos))
		return SL_V92_CP_DECODE_ERROR;
	if (sl_v92_cp_unpack_recovered(&decoder->info, decoder->bits,
				       expected_write_pos) != 0)
		return SL_V92_CP_DECODE_ERROR;
	decoder->info.pad_repeat = decoder->pad_repeat;
	return SL_V92_CP_DECODE_COMPLETE;
}
