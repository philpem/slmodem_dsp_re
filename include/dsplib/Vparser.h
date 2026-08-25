/*
 * Vparser.h -- the parameter-file reader the two parameter blocks call.
 *
 * See src/core/Vparser.c.  Both functions are THREE-BYTE STUBS in the shipped
 * object -- `xor %eax,%eax; ret` -- and their only callers anywhere in `.text`
 * are `V90Parameters::loadParams` and `V92Parameters::loadParams`.  So the
 * shipping build reads no parameter file at all; what survived the build is
 * the CALL SEQUENCE, which is a complete self-describing field map of both
 * classes and the only thing in the object that knows the author's names for
 * those fields (findings F860 and F861).
 *
 * THE RETURN TYPE IS `int` AND IT IS NOT A GUESS.  A `void` function returning
 * nothing compiles to a bare `ret`; both of these zero `%eax` first, which is
 * a value being returned.  What the value MEANS is not recoverable -- nothing
 * in the object inspects it, because every call site discards it -- so the
 * name is `int` and the comment stops there.
 *
 * WHAT IS NOT RECOVERABLE, and is stated here so a reader does not mistake
 * either for a measurement:
 *
 *   - the `const` on the name parameter.  These are C functions with
 *     unmangled names, so no argument type leaves any trace in the object.
 *     `const char *` is chosen because the callers pass string literals and
 *     GCC 3.4.2 warns on the conversion to `char *`; the object cannot tell
 *     the two apart.
 *   - the first parameter's type.  `V90Parameters::loadParams(char *)` passes
 *     its own argument straight through as the first outgoing word, and the
 *     MANGLING types that argument `char *`, so `char *` is forced here by
 *     the caller and not by this function.
 */

#ifndef DSPLIB_VPARSER_H
#define DSPLIB_VPARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read one named integer parameter out of `paramFile` into `*value`.
 *
 * In the shipped object this writes nothing and returns 0 -- see the file
 * comment.  It is declared with the arguments the call sites pass because
 * that is what the object encodes: three words, in this order.
 */
int Vparser_read_int(char *paramFile, const char *name, int *value);

/* The same, for a `float` parameter. */
int Vparser_read_float(char *paramFile, const char *name, float *value);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VPARSER_H */
