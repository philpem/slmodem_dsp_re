/* Constructed component boundary: setters install finite coefficients/betas,
 * enterDataPhase derives maxima and performs the real MMX conversion. No
 * equalizer history, maximum or MMX state is planted. This is not a negotiated
 * modem-session claim. The independent phase4 collaborators are constructed;
 * only their RRN-reset path is used. */
#include <string.h>
#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase4Demodulator.h"
#include "dsplib/V90Equalizer.h"

extern "C" {
extern unsigned ref_dsplibs_debug_level;
void our_ctor(void *,unsigned,unsigned,void *,void *,void *,void *,void *,void *,void *,void *,int)
 asm("_ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4DemodulatorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifierP13V90ParametersP12V90ResamplerP12V90PreFilter20V90ComputationalMode");
void ref_ctor(void *,unsigned,unsigned,void *,void *,void *,void *,void *,void *,void *,void *,int)
 asm("ref__ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4DemodulatorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifierP13V90ParametersP12V90ResamplerP12V90PreFilter20V90ComputationalMode");
void our_dtor(void *) asm("_ZN12V90EqualizerD1Ev");
void ref_dtor(void *) asm("ref__ZN12V90EqualizerD1Ev");
void ref_le(void *,float *,unsigned) asm("ref__ZN12V90Equalizer17setLinearEquCoeffEPfj");
void ref_dfe(void *,float *,unsigned) asm("ref__ZN12V90Equalizer11setDfeCoeffEPfj");
void ref_lebeta(void *,float) asm("ref__ZN12V90Equalizer16setLinearEquBetaEf");
void ref_dfebeta(void *,float) asm("ref__ZN12V90Equalizer10setDfeBetaEf");
int ref_data(void *) asm("ref__ZN12V90Equalizer14enterDataPhaseEv");
}

union Slot { unsigned char bytes[sizeof(V90Equalizer)]; long double align; };
static void compare_state(V90Equalizer *a,V90Equalizer *b,long tag)
{
    /* Owned pointers and allocation-derived alignments differ per side; each
     * of the six written aligned views is compared below. +4c is the separately
     * constructed phase4 collaborator; all other collaborators are shared/null. */
    static const unsigned skip[]={0x14,0x18,0x24,0x28,0x40,0x44,0x4c,0x98,
        0xb4,0xb8,0xd4,0xd8,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,0xf4,
        0x114,0x118,0x11c,0x120,0x124,0x128,0x12c,0x130,0x134};
    unsigned char aa[sizeof(V90Equalizer)],bb[sizeof(V90Equalizer)];
    unsigned i;
    memcpy(aa,a,sizeof(aa));memcpy(bb,b,sizeof(bb));
    for(i=0;i<sizeof(skip)/sizeof(skip[0]);i++) {
        memset(aa+skip[i],0,4);memset(bb+skip[i],0,4);
    }
    diff_eq_obj_( __FILE__,__LINE__,"constructed conversion state","V90Equalizer",aa,bb,sizeof(aa),tag);
#define VIEW(field) diff_eq_obj_( __FILE__,__LINE__,#field,"short[4]",a->field,b->field,4*sizeof(short),tag)
    VIEW(linearEquMmxCoefsAligned);VIEW(array_d8Aligned);VIEW(array_ecAligned);
    VIEW(dfeMmxCoefsAligned);VIEW(array_118Aligned);VIEW(array_12cAligned);
#undef VIEW
}

int main(void)
{
    _tagModemParameters host;
    memset(&host,0,sizeof(host));host.minRate=4800;host.maxRate=33600;
    host.unnamed_005c=1;
    V90Parameters params(&host);
    params.ENABLE_EQUALIZER_MMX=1;
    params.LINEAR_EQU_HISTORY_LENGTH=4;
    V90Phase4Demodulator p4a(0,0,0,0,0,0,0,&params,0,0,0);
    V90Phase4Demodulator p4b(0,0,0,0,0,0,0,&params,0,0,0);
    static const float levels[]={1.0f,3.0f,82.0f};
    static const int shifts[]={1,3,7};
    int level,power,delta,debug,crossings=0,trials=0;
    diff_begin("constructed equalizer reciprocal-scale quantization");
    for(debug=0;debug<=2;debug+=2) for(level=0;level<3;level++) for(power=0;power<3;power++) {
        int first=-999,last=-999;
        for(delta=-1;delta<=1;delta++) {
            Slot x,y;float coefs[4],beta,dbeta;unsigned bits;
            int k,ours,theirs;long tag=trials++;
            memset(&x,0,sizeof(x));memset(&y,0,sizeof(y));
            our_ctor(x.bytes,4,4,0,&p4a,0,0,0,&params,0,0,V90EQU_COMP_MODE_1);
            ref_ctor(y.bytes,4,4,0,&p4b,0,0,0,&params,0,0,V90EQU_COMP_MODE_1);
            V90Equalizer *a=(V90Equalizer *)x.bytes,*b=(V90Equalizer *)y.bytes;
            coefs[0]=levels[level];coefs[1]=levels[level]*0.5f;coefs[2]=-levels[level]*0.25f;coefs[3]=0;
            a->setLinearEquCoeff(coefs,4);ref_le(b,coefs,4);
            a->setDfeCoeff(coefs,4);ref_dfe(b,coefs,4);
            beta=levels[level];dbeta=levels[level];
            for(k=0;k<24+shifts[power];k++) beta*=0.5f;
            for(k=0;k<20+shifts[power];k++) dbeta*=0.5f;
            memcpy(&bits,&beta,4);bits+=delta;memcpy(&beta,&bits,4);
            memcpy(&bits,&dbeta,4);bits+=delta;memcpy(&dbeta,&bits,4);
            a->setLinearEquBeta(beta);ref_lebeta(b,beta);
            a->setDfeBeta(dbeta);ref_dfebeta(b,dbeta);
            dsplibs_debug_level=ref_dsplibs_debug_level=debug;
            dsplib_debug_capture_reset();dsplib_debug_capture_on=1;
            ours=a->enterDataPhase();theirs=ref_data(b);
            dsplib_debug_capture_on=0;
            diff_eq_int("enter return %ld",ours,theirs,tag);
            diff_eq_int("reference converted %ld",theirs,1,tag);
            diff_eq_int("reference mode %ld",b->mmxMode,1,tag);
            diff_eq_float("derived LE maximum",b->maxLeCoefValue,levels[level],tag);
            diff_eq_float("derived DFE maximum",b->maxDfeCoefValue,levels[level],tag);
            compare_state(a,b,tag);
            diff_eq_int("capture complete %ld",dsplib_debug_capture_complete(0)&&dsplib_debug_capture_complete(1),1,tag);
            diff_eq_int("conversion transcript %ld",strcmp(dsplib_debug_capture_text(0),dsplib_debug_capture_text(1)),0,tag);
            diff_eq_int("same normalized reference shift %ld",b->linearEquMmxShift,b->dfeMmxShift,tag);
            if(delta==-1) first=b->linearEquMmxShift;
            if(delta==1) last=b->linearEquMmxShift;
            dsplibs_debug_level=ref_dsplibs_debug_level=0;
            our_dtor(a);ref_dtor(b);
        }
        if(first!=last) crossings++;
    }
    diff_eq_int("constructor trials %ld",trials,54,0);
    diff_eq_int("reference crossed every exponent boundary %ld",crossings,18,0);
    return diff_end();
}
