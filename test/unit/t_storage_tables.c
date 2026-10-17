/* Component data boundaries: setup-selected V.34 coefficient rows and the
 * exported Bell103 detector config passed to its actual constructor. These
 * are valid component calls, not a negotiated modem-session claim. */
#include <string.h>
#include "harness.h"
#include "dsplib/v34filt.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/b103fp.h"

extern void ref_V34SetupModulator(struct v34_modulator *, short, short, short, int, int);
extern const struct fpm_mtd_cfg MTDb103Org_CFG;
extern const struct fpm_mtd_cfg ref_MTDb103Org_CFG;
extern const short ref_MTDb103_COEF[10];
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *, const struct fpm_mtd_cfg *);
extern void ref_FPM_MTD_delete(struct fpm_mtd *);

int main(void)
{
    static const short baud[] = {2400,2800,3000,3200,3429};
    static const short carrier[] = {1600,1680,1800,1920,1959};
    struct v34_modulator a,b;
    short shaped_a[32 * V34_MOD_ROW], shaped_b[32 * V34_MOD_ROW];
    struct fpm_mtd *x,*y;
    int rate,row,rc;
    diff_begin("setup-selected pre-emphasis coefficient rows");
    for (rate=0;rate<5;rate++) for (row=1;row<=10;row++) {
        memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
        a.shaped=shaped_a;b.shaped=shaped_b;
        V34SetupModulator(&a,baud[rate],carrier[rate],row,0,1);
        ref_V34SetupModulator(&b,baud[rate],carrier[rate],row,0,1);
        diff_eq_obj_( __FILE__,__LINE__,"selected row","short[16]",
                      a.preemp,b.preemp,16*sizeof(short),rate*10+row);
    }
    rc=diff_end();
    diff_begin("exported Bell103 detector config constructor");
    diff_eq_int("source config points at its bank (%ld)",MTDb103Org_CFG.coeff==MTDb103_COEF,1,0);
    diff_eq_int("reference config points at its bank (%ld)",ref_MTDb103Org_CFG.coeff==ref_MTDb103_COEF,1,0);
    diff_eq_obj_( __FILE__,__LINE__,"coefficient bank","short[10]",MTDb103_COEF,ref_MTDb103_COEF,10*sizeof(short),0);
    x=FPM_MTD_create(0,&MTDb103Org_CFG);
    y=ref_FPM_MTD_create(0,&ref_MTDb103Org_CFG);
    diff_eq_int("both constructor states exist (%ld)",x!=0 && y!=0,1,0);
    if (x && y) {
        struct fpm_mtd aa=*x,bb=*y;
        diff_eq_int("source constructor copied bank (%ld)",x->cfg.coeff==MTDb103_COEF,1,0);
        diff_eq_int("reference constructor copied bank (%ld)",y->cfg.coeff==ref_MTDb103_COEF,1,0);
        aa.cfg.coeff=bb.cfg.coeff=0;aa.acc=bb.acc=0;
        diff_eq_obj("constructor state",struct fpm_mtd,&aa,&bb,0);
        diff_eq_obj_( __FILE__,__LINE__,"constructor accumulator","short[4]",x->acc,y->acc,4*sizeof(short),0);
    }
    if (x) FPM_MTD_delete(x);
    if (y) ref_FPM_MTD_delete(y);
    rc|=diff_end();
    return rc;
}
