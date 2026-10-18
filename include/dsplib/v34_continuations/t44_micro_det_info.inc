
	struct v34_bitsource *rec;
	short nbits, count, next;
	int idx;

	obj->short_aa7a = 0;

	/* No bit waiting: the arm is over before it starts. */
	nbits = obj->fsk.nbits;
	if (nbits == 0) {
		t3c_txblock(obj);			/* 0x6ab88 */
		V34_CONT_RETURN();
	}

	/* 0x668e1 reads the record BEFORE 0x668e7 writes the count back. */
	rec = (struct v34_bitsource *)obj->paa70;
	obj->fsk.nbits = (short)(nbits - 1);

	count = obj->short_aa78;

	/*
	 * The message's own bits go through the register; the sixteen after
	 * it are the CRC and do not.  One bit per call, MSB first:
	 *
	 *   668ff  movswl %ax,%edx / shr $0x1f,%edx    the register's bit 15
	 *   66907  testb  $0x1,0xaae2(%edi)            the incoming bit
	 *   66910  xor    $0x1,%edx                    so %edx is their XOR
	 *   66922  xor    $0x1021,%eax                 taken only when it is 1
	 */
	if (count < rec->nbits) {
		unsigned crc = (unsigned short)rec->crc;
		int fb = (int)(crc >> 15);

		if ((obj->fsk.sr & 1) != 0)
			fb ^= 1;
		crc <<= 1;
		/*
		 * The `else` is a whole out-of-line block, not a fallthrough:
		 * `mov %ax,0x14(%ecx)` exists twice, at 0x66927 after the xor
		 * and at 0x6c133 without it; `je 6c133` at 0x66915 picks it.
		 * The shift is unconditional -- 0x66905 `add %eax,%eax` runs
		 * before the test.
		 */
		if (fb != 0)
			crc ^= 0x1021;			/* else 0x6c133 */
		rec->crc = (short)crc;
	}

	next = (short)(count + 1);
	obj->short_aa78 = next;

	/*
	 * Sixteen CRC bits after the message: 0x66944 re-reads the length.
	 * 0x6bda4 then compares the register against what arrived, sixteen
	 * bits wide, and equal is the message accepted.
	 */
	if ((int)next == (int)rec->nbits + 0x10) {
		if ((unsigned short)obj->fsk.sr
		    == (unsigned short)rec->crc) {
			#ifdef V34_ROOT_CONTINUATIONS
{
                int v34_accept_result;
                {
                    short count2 = next;
#undef V34_CONT_RETURN
#define V34_CONT_RETURN(value) do { v34_accept_result = (value); goto v34_accept_done; } while (0)
#include "dsplib/v34_continuations/t44_det_info_accept.inc"
#undef V34_CONT_RETURN
#define V34_CONT_RETURN(value) return value
v34_accept_done:
                    ;
                }
                if (v34_accept_result != 0)
                    V34_CONT_RETURN();
            }
#else
if (t44_det_info_accept(obj, rec, next) != 0)
				V34_CONT_RETURN();
#endif			/* 0x6e740 */
		} else {
			/*
			 * The restart is the fallthrough of `je 6e534` at
			 * 0x6bdab, and 0x6bdb1 is its FIRST instruction: the
			 * debug-level test.  The compare feeding it is
			 * 0x6bda4, sixteen bits, +0xaae2 against +0x14.
			 */
			t44_det_info_restart(obj);	/* 0x6bdb1 */
		}
	}

	/*
	 * 0x6695d RE-READS THE COUNTER, and it is not the value stored at
	 * 0x6693d.  The restart leaves +0xaa78 alone, so on that path the two
	 * agree -- but the accept path resets it at 0x6e750 and may reload it
	 * from +0xaa7c at 0x6e8ac, and everything below runs on what is in
	 * memory.  A reset counter therefore makes the whole byte clock a
	 * no-op on the call that accepted a message, which is not what a
	 * cached local would do and is how the object was caught saying so.
	 */
	next = obj->short_aa78;

	/* Only every eighth bit completes a byte. */
	if ((next & 7) != 0) {
		/*
		 * 0x66966 `jne 6bd8d`, and the test at 0x66964 is
		 * `test $0x7,%al` -- ONE BYTE of a counter the source
		 * carries as a short.
		 */
		t3c_txblock(obj);			/* 0x6bd8d */
		V34_CONT_RETURN();
	}
	if (next <= 0) {
		/*
		 * 0x6696c `test %ax,%ax` + `jle 7186a`: sixteen bits and
		 * SIGNED, so a negative counter leaves here too.
		 */
		t3c_txblock(obj);			/* 0x7186a */
		V34_CONT_RETURN();
	}

	/*
	 * Ten bytes and no more.  0x6697a re-signs the index through sixteen
	 * bits before comparing, which cannot change it here -- `next` is
	 * positive and a multiple of eight -- but the object does it, so the
	 * cast stays where the compare is.
	 */
	idx = ((int)next >> 3) - 1;
	if ((short)idx > 9) {
		t3c_txblock(obj);			/* 0x71857 */
		V34_CONT_RETURN();
	}

	/*
	 * 0x6698e re-reads +0xaa70: the restart above may have run in
	 * between, and it is the LOW BYTE of `sr` that is stored, widened to
	 * a short.
	 */
	rec = (struct v34_bitsource *)obj->paa70;
	rec->word[idx] = (short)(unsigned char)obj->fsk.sr;

	t3c_txblock(obj);
