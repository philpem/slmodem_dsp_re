/* Fixed initialized-component boundary: a 10:9 filter can produce >32767
 * samples from a valid positive signed-short input count. The legacy short
 * reference alias hid that result bit. Read the blob's full EAX result here;
 * its two exits explicitly zero-extend the low word. This is a resampler leaf
 * fixture, not evidence of a public modem request this large. At32768
 * outputs the last index written is32767; a larger count wraps the blob
 * output index before another store and is deliberately excluded. */
#include "harness.h"
#include "dsplib/fpm_mrf.h"

extern void ref_FPM_MRF_init(void *, const void *, int);
extern void ref_FPM_MRF_free(void *);
extern unsigned int ref_FPM_MRF_filter(void *, const short *, short *, short);
extern const short ref_B103_MRF_FILT_TX[270];

#define INPUT_CAP 32767
#define OUTPUT_CAP 40000
static short input[INPUT_CAP];
static short ours_out[OUTPUT_CAP], blob_out[OUTPUT_CAP];

int main(void)
{
 static const short counts[] = {29490,29491};
 static const unsigned expected[] = {32767,32768};
 struct fpm_mrf_cfg cfg={10,9,ref_B103_MRF_FILT_TX,270,0,0};
 int i,trial,pattern,rc=0;
 for(pattern=0;pattern<2;pattern++){
 for(i=0;i<INPUT_CAP;i++)input[i]=pattern ? 0 : (short)(i%2001-1000);
 for(trial=0;trial<2;trial++){
  struct fpm_mrf ours,blob,normalized_ours,normalized_blob;
  int got;unsigned want,k;
  memset(&ours,0,sizeof(ours));memset(&blob,0,sizeof(blob));
  memset(ours_out,0x5a,sizeof(ours_out));memset(blob_out,0x5a,sizeof(blob_out));
  FPM_MRF_init(&ours,&cfg,1);ref_FPM_MRF_init(&blob,&cfg,1);
  diff_begin("MRF initialized output-count boundary");
  diff_eq_int("constructed history length",ours.history_len,27,counts[trial]);
  got=FPM_MRF_filter(&ours,input,ours_out,counts[trial]);
  want=ref_FPM_MRF_filter(&blob,input,blob_out,counts[trial]);
  printf("  pattern=%d input=%d reconstructed=%d blob=%u\n",pattern,counts[trial],got,want);
  diff_eq_int("blob count geometry",want,expected[trial],counts[trial]);
  diff_eq_int("full output count",got,want,counts[trial]);
  normalized_ours=ours;normalized_blob=blob;
  normalized_ours.history=normalized_blob.history=0;
  diff_eq_obj("after filter",struct fpm_mrf,&normalized_ours,&normalized_blob,counts[trial]);
  for(k=0;k<OUTPUT_CAP;k++)diff_eq_int("output and untouched tail",ours_out[k],blob_out[k],k);
  for(i=0;i<27;i++)diff_eq_int("history",ours.history[i],blob.history[i],i);
  rc|=diff_end();
  FPM_MRF_free(&ours,1);ref_FPM_MRF_free(&blob);
 }
 }
 printf("MRF result boundary: 4 paired initialized calls; 2 high-bit results\n");
 return rc;
}
