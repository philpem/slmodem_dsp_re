/*
 * ringdet.h -- the software ring detector's state and configuration, as far
 * as RingDetector_Reset establishes them (blob span `voice.c#3`).
 *
 * The field names on the left column of both structs come from the object's
 * own debug line --
 *
 *   "\n Reset Soft Ring: Threshold = %d, Fs = %d, MinFreq = %d, MaxFreq =%d
 *    \n\tminOnDur = %d, minOffDur = %d \n"
 *
 * -- via the values Reset prints (tier-1 evidence, the author's words).
 * Everything else Reset merely zeroes or seeds, so those keep neutral
 * type_offset names until the detector itself is reconstructed.
 */

#ifndef DSPLIB_RINGDET_H
#define DSPLIB_RINGDET_H

#ifdef __cplusplus
extern "C" {
#endif

struct ring_detector_cfg {
	int	fs;		/* +0x00 printed as "Fs"         */
	int	min_freq;	/* +0x04 printed as "MinFreq"    */
	int	max_freq;	/* +0x08 printed as "MaxFreq"    */
	int	min_on_dur;	/* +0x0c printed as "minOnDur"   */
	int	min_off_dur;	/* +0x10 printed as "minOffDur"  */
	int	threshold;	/* +0x14 printed as "Threshold";
				 *       sign selects a mode      */
};

struct ring_detector {
	int	min_freq;	/* +0x00 cfg, clamped >= 14        */
	int	max_freq;	/* +0x04 cfg, clamped <= 100       */
	int	min_on_dur;	/* +0x08 cfg, clamped >= 40        */
	int	min_off_dur;	/* +0x0c cfg, clamped >= 120       */
	int	fs;		/* +0x10 */
	int	int_14;		/* +0x14 zeroed                    */
	int	int_18;		/* +0x18 zeroed                    */
	int	int_1c;		/* +0x1c zeroed                    */
	int	int_20;		/* +0x20 zeroed                    */
	int	int_24;		/* +0x24 NOT touched by Reset      */
	int	int_28;		/* +0x28 zeroed                    */
	short	short_2c;	/* +0x2c zeroed                    */
	short	short_2e;	/* +0x2e zeroed                    */
	short	short_30;	/* +0x30 |threshold| / (fs/80)     */
	short	short_32;	/* +0x32 2, or 0 when threshold<0  */
	short	short_34;	/* +0x34 100, or 200 when thr<0    */
	short	short_36;	/* +0x36 |threshold|               */
	short	short_38;	/* +0x38 3*fs / (4*cfg min_freq)   */
	short	short_3a;	/* +0x3a zeroed                    */
	short	short_3c;	/* +0x3c zeroed                    */
	short	short_3e;	/* +0x3e set to 1                  */
	short	short_40;	/* +0x40 zeroed                    */
	short	short_42;	/* +0x42 zeroed                    */
	short	short_44;	/* +0x44 |threshold|               */
	short	short_46;	/* +0x46 -|threshold|              */
	short	short_48;	/* +0x48 zeroed                    */
	short	short_4a;	/* +0x4a zeroed                    */
	short	short_4c;	/* +0x4c zeroed                    */
	short	short_4e;	/* +0x4e copy of short_30          */
	short	short_50;	/* +0x50 copy of short_30          */
};

void RingDetector_Reset(struct ring_detector *s,
			struct ring_detector_cfg *c);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_RINGDET_H */
