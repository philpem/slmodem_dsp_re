/*
 * V90CodecType.h -- `__tHardwareCodecTypes__`, and nothing else.
 *
 * WHY A HEADER FOR ONE ENUM.  Two translation units need this type and they
 * cannot share a bigger header.  `V90PreFilter.h` spells the class the type is
 * mostly used with; `V90ModemCtor.cpp` names the type in the mangled
 * declaration of `V90Demodulator::V90Demodulator` -- and that file must NOT
 * have `V90PreFilter.h`, because there are two definitions of `V90Parameters`
 * (finding 1112) and V90PreFilter.h carries the 0x504 block form while this
 * constructor allocates the 0x558 named one.  See V90ModemCtor.cpp's own
 * header comment, which lays the trade out in full.
 *
 * The type used to be spelled in both places.  That was legal only because it
 * was an OPAQUE DECLARATION -- `enum __tHardwareCodecTypes__ : int;` -- which
 * may be repeated.  That spelling is C++11, the author's compiler was C++98,
 * and the C++98 replacement is a definition, which may not.  So the type gets
 * one home, and this is the only home small enough for both consumers to
 * include.
 *
 * `_BASE_PIN` IS OURS.  The object names no enumerator: the mangling of the
 * constructor and of `setFilter` records the type's NAME
 * (`23__tHardwareCodecTypes__`) and nothing about its contents, so naming an
 * enumerator would be putting a guess into the record.  What the pin fixes is
 * the underlying type, and only that -- a single negative enumerator makes the
 * base signed and every `int` representable, which the type needs because
 * V90ModemCtor.cpp casts a RUNTIME int to it: `_tagModemParameters::codecType`
 * at +0x54, loaded with a plain `mov 0x54(%edx),%ecx` and passed straight on.
 * An empty enum would have the range 0..0 and make that cast undefined.
 *
 * Measured identical to the C++11 spelling -- 4 bytes, signed, same mangling
 * -- under GCC 3.4.2 and GCC 13 both.  docs/method/compilers.md, V2.
 */

#ifndef DSPLIB_V90CODECTYPE_H
#define DSPLIB_V90CODECTYPE_H

enum __tHardwareCodecTypes__ { __tHardwareCodecTypes___BASE_PIN = -0x7fffffff - 1 };

typedef char v90codectype_is_signed[
    ((enum __tHardwareCodecTypes__)-1 < (enum __tHardwareCodecTypes__)0) ? 1 : -1];

#endif /* DSPLIB_V90CODECTYPE_H */
