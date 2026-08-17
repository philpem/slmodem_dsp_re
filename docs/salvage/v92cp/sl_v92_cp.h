/*
 * Recovered SmartLink V92CP control-packet packing and decoding.
 *
 * This module is intentionally separate from V.8. V.8 can select DP_V92 after
 * the recovered six-word PCM preamble, but the later V.92 control-packet
 * generators live below the V.PCM/V.90/V.92 boundary.
 */

#ifndef SL_RE_V92_CP_H
#define SL_RE_V92_CP_H

#include <stdint.h>

#define SL_V92_CP_MAX_DESCRIPTOR_GROUPS 6U
#define SL_V92_CP_WORDS_PER_DESCRIPTOR 8U
#define SL_V92_CP_MAX_BITS 2048U

enum sl_v92_cp_decode_status {
	SL_V92_CP_DECODE_ERROR = -1,
	SL_V92_CP_DECODE_PENDING = 0,
	SL_V92_CP_DECODE_COMPLETE = 1,
};

struct sl_v92_cp_info {
	unsigned state;
	unsigned repeat;
	unsigned info_byte;
	unsigned section_tag;
	unsigned tail_flag;
	unsigned suv;
	unsigned state1_code;
	unsigned session_flags;
	unsigned scalar_flags;
	float scalar_magnitude;
	float signed_scalar[4];
	uint16_t short_scalar_a[4];
	uint16_t short_scalar_b[2];
	unsigned pad_repeat;
	unsigned descriptor_count;
	unsigned secondary_enabled;
	uint16_t primary_descriptor[SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				   [SL_V92_CP_WORDS_PER_DESCRIPTOR];
	uint16_t secondary_descriptor[SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				     [SL_V92_CP_WORDS_PER_DESCRIPTOR];
};

struct sl_v92_cp_bits {
	uint8_t bits[SL_V92_CP_MAX_BITS];
	unsigned bit_count;
	unsigned write_pos;
	unsigned padded_count;
};

struct sl_v92_cp_decoder {
	uint8_t bits[SL_V92_CP_MAX_BITS];
	unsigned bit_count;
	unsigned expected_write_pos;
	unsigned pad_repeat;
	struct sl_v92_cp_info info;
};

struct sl_v92_cp_descriptor_input {
	const uint8_t *primary_codes;
	const uint8_t *secondary_codes;
	unsigned code_count;
};

struct sl_v92_cp_descriptor_plan {
	unsigned unique_count;
	unsigned source_to_unique[SL_V92_CP_MAX_DESCRIPTOR_GROUPS];
	uint16_t primary_descriptor[SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				   [SL_V92_CP_WORDS_PER_DESCRIPTOR];
	uint16_t secondary_descriptor[SL_V92_CP_MAX_DESCRIPTOR_GROUPS]
				     [SL_V92_CP_WORDS_PER_DESCRIPTOR];
};

struct sl_v92_cp_params {
	unsigned info_source;
	unsigned session_flags;
	unsigned scalar_flags;
	unsigned secondary_enabled;
	float signed_scalar[4];
	struct sl_v92_cp_descriptor_input descriptors[SL_V92_CP_MAX_DESCRIPTOR_GROUPS];
};

struct sl_v92_cp_unpacked_params {
	unsigned info_source;
	unsigned session_flags;
	unsigned scalar_flags;
	unsigned secondary_enabled;
	float signed_scalar[4];
	unsigned source_to_unique[SL_V92_CP_MAX_DESCRIPTOR_GROUPS];
	unsigned code_count[SL_V92_CP_MAX_DESCRIPTOR_GROUPS];
	uint8_t primary_codes[SL_V92_CP_MAX_DESCRIPTOR_GROUPS][128];
	uint8_t secondary_codes[SL_V92_CP_MAX_DESCRIPTOR_GROUPS][128];
};

struct sl_v92_cp_request {
	unsigned tail_flag;
	unsigned repeat;
	unsigned section_tag;
	unsigned pad_repeat;
	float scalar_magnitude;
	unsigned suv;
};

void sl_v92_cp_clear_descriptor(uint16_t descriptor
				[SL_V92_CP_WORDS_PER_DESCRIPTOR]);
int sl_v92_cp_build_descriptor(uint16_t descriptor
			       [SL_V92_CP_WORDS_PER_DESCRIPTOR],
			       const uint8_t *codes, unsigned count);
int sl_v92_cp_descriptor_to_codes(const uint16_t descriptor
				  [SL_V92_CP_WORDS_PER_DESCRIPTOR],
				  uint8_t *codes, unsigned capacity,
				  unsigned *count);
int sl_v92_cp_build_descriptor_plan(struct sl_v92_cp_descriptor_plan *plan,
				    const struct sl_v92_cp_descriptor_input *groups,
				    unsigned group_count);
int sl_v92_cp_apply_descriptor_plan(struct sl_v92_cp_info *info,
				    const struct sl_v92_cp_descriptor_plan *plan,
				    unsigned secondary_enabled);
int sl_v92_cp_info_from_params(struct sl_v92_cp_info *info,
			       const struct sl_v92_cp_params *params,
			       const struct sl_v92_cp_request *request);
int sl_v92_cp_params_from_info(struct sl_v92_cp_unpacked_params *params,
			       const struct sl_v92_cp_info *info);
int sl_v92_cp_pack_recovered(const struct sl_v92_cp_info *info,
			     struct sl_v92_cp_bits *out);
int sl_v92_cp_unpack_recovered(struct sl_v92_cp_info *info,
			       const uint8_t *bits, unsigned bit_count);
void sl_v92_cp_decoder_reset(struct sl_v92_cp_decoder *decoder,
			     unsigned pad_repeat);
int sl_v92_cp_decoder_feed(struct sl_v92_cp_decoder *decoder, uint8_t bit);

#endif
