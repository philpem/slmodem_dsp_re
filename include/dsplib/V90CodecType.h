/**
 * @file V90CodecType.h
 * @brief `enum __tHardwareCodecTypes__`, the hardware-codec-type tag shared
 *        by `V90PreFilter.h` and `V90ModemCtor.cpp`, isolated in its own
 *        header.
 *
 * Two translation units need this type: `V90PreFilter.h` spells the class
 * the type is mostly used with, while `V90ModemCtor.cpp` names the type in
 * the mangled declaration of `V90Demodulator::V90Demodulator`. Splitting it
 * into its own minimal header used to be load-bearing rather than tidy --
 * `V90PreFilter.h` once carried a second, incompatible `V90Parameters`
 * definition (finding F1112) that `V90ModemCtor.cpp` could not have in its
 * translation unit alongside the named one in `V90Parameters.h` -- but that
 * duplication was retired at task #116 (finding F6402); `V90ModemCtor.cpp`
 * now includes `V90Demodulator.h` (which reaches `V90PreFilter.h`) and
 * `V90Parameters.h` together without conflict. The type still gets its own
 * header because of the C++98 point below, which is independent of F1112.
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
