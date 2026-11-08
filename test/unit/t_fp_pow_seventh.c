/* Synthetic period-machine table context, not historical neighboring values. */
#ifndef DSPLIB_REPRODUCE_BUGS
#error "FP_Pow memory-context fixture requires reproduction mode"
#endif
#include "harness.h"
extern int FP_Pow(int);
extern int ref_FP_Pow(int);
short fp_pow_probe_backing[7] = {16384,8192,2730,683,136,23,3};
int main(void)
{
    static const int input[4] = {16383,-16383,16384,-16384};
    static const short seventh[2] = {-7,19};
    /* For seventh word s, inputs below give respectively:
     * 44526+floor(16377*s/16384), 6030+floor(-16380*s/16384),
     * 44532+s and 6032-s. The native six-word tables remain unchanged. */
    static const int expected[2][4] = {
        {44519,6036,44525,6039}, {44544,6011,44551,6013}
    };
    int i,j,a,b;
    diff_begin("FP_Pow synthetic seventh-word context");
    for(j=0;j<2;j++) {
        fp_pow_probe_backing[6]=seventh[j];
        for(i=0;i<4;i++) {
            a=FP_Pow(input[i]); b=ref_FP_Pow(input[i]);
            diff_eq_int("reference boundary",b,expected[j][i],j*4+i);
            diff_eq_int("candidate boundary",a,expected[j][i],j*4+i);
            diff_eq_int("paired boundary",a,b,j*4+i);
        }
    }
    return diff_end();
}
