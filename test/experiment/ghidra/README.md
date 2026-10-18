# Ghidra x87 calibration

These synthetic functions distinguish explicit product rounding from an
unrounded x87 product, and provide three final-operation controls. They are
analysis apparatus. Keep the instruction sequences fixed when testing a
new Ghidra version.

From the repository root, assemble with the period toolchain, then execute
the native 32-bit check. The host compiler only builds the driver; the
operations being calibrated are in the period-assembled fixture.

```sh
docker run --rm --platform linux/386 -v "$PWD:/src" -w /src \
  ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  /bin/sh -c 'gcc -DDSPLIB_REPRODUCE_BUGS -c test/experiment/ghidra/x87.S -o build/x87-calibration.o'
gcc -m32 -O0 -no-pie test/experiment/ghidra/x87_check.c \
  build/x87-calibration.o -o build/x87-calibration-check
build/x87-calibration-check
objdump -d build/x87-calibration.o
```

The check must report `5/5` and different narrow/wide float words:
`c0920eaa` and `c0920eab`. The other four checks demonstrate that the final
arithmetic operations affect the returned values.

Import the object as ELF x86-32/GCC. Export all four functions before adding
annotations. Then explicitly set each signature to
`float name(float a, float b, float c)` with `__cdecl`, invalidate the
function's decompiler cache, and export them again. Record the actual
Ghidra version, bytes and program paths, both inferred and annotated
signatures, pseudocode, P-code level and arithmetic widths.

With the current MCP bridge, POST parameters use a JSON body. Its signature
parser accepts the prototype without a trailing semicolon. Always include
the explicit program path when several programs are open.

Count every control, including errors and unsupported results. Empty output
is invalid for these functions. Identical high P-code must not override the
native difference. The [campaign record](../../../docs/method/ghidra-assisted.md)
contains the measured Ghidra 12.2 results and bridge limitations.
