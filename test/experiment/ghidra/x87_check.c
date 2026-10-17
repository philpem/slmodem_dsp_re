/* Native execution checks for the synthetic Ghidra x87 calibration fixture.
 * This is measurement apparatus, not reconstructed modem source. */
#include <stdio.h>

extern float calibration_nsub(float, float, float);
extern float calibration_nadd(float, float, float);
extern float calibration_ndiv(float, float, float);
extern float calibration_wide(float, float, float);

int main(void)
{
    union { float value; unsigned int bits; } narrow, wide;
    unsigned passed = 0;
    passed += calibration_nsub(2.0f, 3.0f, 1.0f) == 5.0f;
    passed += calibration_nadd(2.0f, 3.0f, 1.0f) == 7.0f;
    passed += calibration_ndiv(2.0f, 3.0f, 1.0f) == 6.0f;
    passed += calibration_wide(2.0f, 3.0f, 1.0f) == 5.0f;
    narrow.value = calibration_nsub(2.72150183f, -0.844936609f, 2.26479387f);
    wide.value = calibration_wide(2.72150183f, -0.844936609f, 2.26479387f);
    passed += narrow.bits != wide.bits;
    printf("x87 calibration: %u/5 checks; narrow=%08x wide=%08x\n",
           passed, narrow.bits, wide.bits);
    return passed != 5;
}
