/*
 * V90ConstellationPower.cpp -- the constructor and destructor, which are a
 * `ret` each.
 *
 * `include/dsplib/V90ConstellationPower.h` carries the object map, the
 * 144-byte measurement and the argument for declaring two empty bodies.
 *
 * PLAIN CDECL with `this` as the first STACK argument, like the rest of the
 * C++ here (finding 215).  Neither body touches it, so this file's whole
 * claim is the four one-byte symbols and the size of the object they leave
 * alone.
 */

#include <stddef.h>

#include "dsplib/V90ConstellationPower.h"

/*
 * Hold the compiler to the size in the header.  There are no fields to assert
 * offsets for, so the size is the only assertion this file can make -- and it
 * is the one that matters, because it is what the test's `diff_eq_obj` spans.
 *
 * Not guarded on the pointer width: the object as modelled is a byte array,
 * so its size does not move between the 32-bit build and `make check64`.
 */
typedef char v90cp_size[(sizeof(V90ConstellationPower) == 0x90) ? 1 : -1];

V90ConstellationPower::V90ConstellationPower()
{
}

V90ConstellationPower::~V90ConstellationPower()
{
}
