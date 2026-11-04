/* Synthetic period-machine address-semantics probe for original D302.
 * Both emitted lookup operands bind to this allocated backing. This is
 * neither an ISO-C defined-behaviour claim nor historical outer-ring data. */
#ifndef DSPLIB_REPRODUCE_BUGS
#error "D302 machine-fidelity fixture requires reproduction mode"
#endif
#include "harness.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fse.h"
extern unsigned short ref_FSE_decision_16pt(void *,short *,short *);
extern short ref_DECv32_IMAP16[16],ref_DECv32_QMAP16[16];
extern short ref_DECv32_ANGL9600[16];
extern const short ref_SMCv32_IMAP16[17],ref_SMCv32_QMAP16[17];
short d302_backing[12288];
static const short values[3]={1111,2222,3333};
static int point(short angle,unsigned short result)
{
 int k,j,found=-1;
 for(k=0;k<16;k++)if(ref_DECv32_ANGL9600[k]==angle){
  for(j=0;j<16;j++)if(ref_DECv32_IMAP16[k]==ref_SMCv32_IMAP16[j]&&
      ref_DECv32_QMAP16[k]==ref_SMCv32_QMAP16[j]&&(j&3)==(result&3)){
   if(found!=-1)return -1;found=k;
  }
 }
 return found;
}
int main(void)
{
 struct fpm_fse a,b;struct v32_dec da,db;
 short ai[1],aq[1],bi[1],bq[1];
 int i,k,ring,trial=0,seen[3]={0,0,0}; unsigned seed=20261104u;
 for(i=0;i<12288;i++)d302_backing[i]=0x5a5a;
 d302_backing[0]=5792;d302_backing[1]=12953;d302_backing[2]=17378;
 for(i=0;i<3;i++)d302_backing[(i+1)*4096-1]=values[i];
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 memset(&da,0,sizeof(da));memset(&db,0,sizeof(db));
 a.cfg.owner=&da;b.cfg.owner=&db;
 a.out_i=ai;a.out_q=aq;b.out_i=bi;b.out_q=bq;
 diff_begin("D302 synthetic shared backing");
 for(i=0;i<512;i++){
  short aa=0,ba=0,am=-123,bm=-123;unsigned short ar,br;
  seed=seed*1664525u+1013904223u;ai[0]=bi[0]=(short)(seed>>16);
  seed=seed*1664525u+1013904223u;aq[0]=bq[0]=(short)(seed>>16);
  br=ref_FSE_decision_16pt(&b,&ba,&bm);
  ar=FSE_decision_16pt(&a,&aa,&am);
  k=point(ba,br);diff_eq_int("unique native point",k>=0,1,trial);
  if(k<0)return 1;
  ring=((ref_DECv32_IMAP16[k]<0?-ref_DECv32_IMAP16[k]:ref_DECv32_IMAP16[k])+
        (ref_DECv32_QMAP16[k]<0?-ref_DECv32_QMAP16[k]:ref_DECv32_QMAP16[k]))/8192-1;
  diff_eq_int("native ring bounded",ring>=0&&ring<3,1,trial);
  if(ring<0||ring>2)return 1;
  seen[ring]++;
  diff_eq_int("reference address value",bm,values[ring],trial);
  diff_eq_int("magnitude",am,bm,trial);
  diff_eq_int("angle",aa,ba,trial);diff_eq_int("decision",ar,br,trial);
  diff_eq_obj("decoder",struct v32_dec,&da,&db,trial);
  diff_eq_int("quality mu",a.mu_sel,b.mu_sel,trial);
  diff_eq_int("quality LMS",a.lms_on,b.lms_on,trial);
  trial++;
 }
 for(i=0;i<3;i++)diff_eq_int("ring exercised",seen[i]>0,1,i);
 diff_eq_int("trial denominator",trial,512,0);
 printf("D302 controlled backing: %d paired calls; rings %d/%d/%d; 3 distinct values\n",trial,seen[0],seen[1],seen[2]);
 return diff_end();
}
