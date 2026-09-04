/**
 * @file V90CodecType.h
 * @brief `enum __tHardwareCodecTypes__`, the hardware-codec-type tag shared
 *        by `V90PreFilter.h` and `V90ModemCtor.cpp`, isolated in its own
 *        header.
 *
 * Two translation units need this type and cannot share a bigger header:
 * `V90PreFilter.h` spells the class the type is mostly used with, while
 * `V90ModemCtor.cpp` names the type in the mangled declaration of
 * `V90Demodulator::V90Demodulator` and must NOT include `V90PreFilter.h`,
 * because there are two definitions of `V90Parameters` (finding F1112) and
 * `V90PreFilter.h` carries the 0x504 block form while that constructor
 * allocates the 0x558 named one -- see `V90ModemCtor.cpp`'s own header
 * comment for the full trade-off. So this type gets its own minimal home.
 *
 * The type used to be spelled in both places as the C++11 opaque
 * declaration `enum __tHardwareCodecTypes__ : int;`, which may legally
 * repeat. The author's compiler was C++98, where the equivalent spelling is
 * a definition and may not repeat, so the type now has exactly one home
 * here.
 *
 * `__tHardwareCodecTypes___BASE_PIN` is our own enumerator, not the
 * object's: the mangling of the constructor and of `setFilter` records
 * only the type's name (`23__tHardwareCodecTypes__`), never its contents,
 * so naming a real enumerator would record a guess. The pin exists solely
 * to fix the underlying type -- a single negative enumerator makes the
 * base signed and every `int` representable, which the type needs because
 * `V90ModemCtor.cpp` casts a runtime int to it
 * (`_tagModemParameters::codecType` at +0x54, loaded with a plain
 * `mov 0x54(%edx),%ecx` and passed straight through). An empty enum would
 * have range 0..0 and make that cast undefined.
 *
 * Measured identical to the C++11 spelling -- 4 bytes, signed, same
 * mangling -- under both GCC 3.4.2 and GCC 13. See docs/method/compilers.md,
 * deviation V2.
 */

#ifndef DSPLIB_V90CODECTYPE_H
#define DSPLIB_V90CODECTYPE_H

enum __tHardwareCodecTypes__ { __tHardwareCodecTypes___BASE_PIN = -0x7fffffff - 1 };

typedef char v90codectype_is_signed[
    ((enum __tHardwareCodecTypes__)-1 < (enum __tHardwareCodecTypes__)0) ? 1 : -1];

#endif /* DSPLIB_V90CODECTYPE_H */
