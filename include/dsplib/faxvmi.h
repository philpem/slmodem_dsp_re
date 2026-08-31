/*
 * faxvmi.h -- Class 1 fax: the Virtual Modem Interface, as far as this
 * batch reads it.
 *
 * FAXVMI wraps one modulation behind a numbered slot: +0x0e holds the slot
 * index and +0x28 the wrapped modem's handle, and the FAXVMI entry points
 * dispatch through parallel 13-entry tables (`vxx_message`, `vxx_status`,
 * ...) indexed by that slot.  The slot map, read off vxx_message's
 * relocations: 0..4 null, 5 v21tx, 6 v21rx, 7 v27tx, 8 v27rx, 9 v29tx,
 * 10 v29rx, 11 v17tx, 12 v17rx.
 *
 * Only `FAXVMI_message` is reconstructed -- it is finding F8320's
 * no-entry-point bucket -- so only the two fields it reads are modelled;
 * the fax phase owns the rest (`FAXVMI_status` also reads +0x18, +0x1c,
 * +0x24).
 */

#ifndef DSPLIB_FAXVMI_H
#define DSPLIB_FAXVMI_H

struct faxvmi {
	unsigned char pad_00[0x0e];	/* +0x00                            */
	short slot;			/* +0x0e index into the vxx tables  */
	unsigned char pad_10[0x18];	/* +0x10                            */
	void *handle;			/* +0x28 the wrapped modem          */
};

/*
 * The dispatch contract every vxx_* table entry follows: (handle, code,
 * out-parameter).  The message form answers a string or NULL.
 */
typedef void (*faxvmi_message_fn)(void *handle, int code, char **out);

/* 13 slots; `.rodata` in the object, so `const` here. */
extern faxvmi_message_fn const vxx_message[13];

/*
 * Ask the wrapped modem for `code`'s message string.  NULL for a code the
 * modulation does not name.  `code` really is an unsigned char in the
 * object -- it is loaded with movzbl -- where the table entries take an
 * int; the narrowing is the author's.
 */
char *FAXVMI_message(struct faxvmi *vmi, unsigned char code);

#endif /* DSPLIB_FAXVMI_H */
