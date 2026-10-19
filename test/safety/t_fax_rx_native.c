/* Native component histories through the real Class 1 RX dispatcher and DSP. */
#include <stdio.h>
#include <string.h>
#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/faxvmi.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29fax.h"
static int checks, failures;
static void check(const char *name, int ok)
{
 ++checks;
 if (!ok) { ++failures; fprintf(stderr, "FAIL: %s\n", name); }
}
static void history(int rate, int mode)
{
 struct fax_class1 ctx;
 memset(&ctx, 0, sizeof(ctx));
 ctx.rx_agc_mult = 0x1234; ctx.rx_agc_shift = 7; ctx.f12d4 = 0x4321;
 _init_receiver(&ctx, rate);
 check("created receiver", ctx.current_mod == mode && ctx.vmi_b != 0);
 if (mode == 0) {
  struct v27_rx *p = ctx.vmi_b->link->modem;
  check("V27 gains/control", p->rx->agc.mult == 0x1234 && p->rx->agc.shift == 7 && p->shared->v21_watch == 0x4321);
 } else if (mode == 1) {
  struct v29_rx *p = ctx.vmi_b->link->modem;
  check("V29 gains/control", p->rx->agc.mult == 0x1234 && p->rx->agc.shift == 7 && p->det->gate_1c == 0x4321);
 } else {
  struct v17rx *p = ctx.vmi_b->link->modem;
  check("V17 gains/control", p->state->agc.value.mult == 0x1234 && p->state->agc.value.shift == 7 && p->ctl->r20 == 0x4321);
 }
 ctx.f12d4 = 0x3456;
 _init_receiver(&ctx, rate);
 check("reinitialized receiver", ctx.current_mod == mode && ctx.vmi_b != 0);
 if (mode == 0) {
  struct v27_rx *p = ctx.vmi_b->link->modem;
  check("V27 reinit control", p->shared->v21_watch == 0x3456);
 } else if (mode == 1) {
  struct v29_rx *p = ctx.vmi_b->link->modem;
  check("V29 reinit control", p->det->gate_1c == 0x3456);
 } else {
  struct v17rx *p = ctx.vmi_b->link->modem;
  check("V17 reinit low short", p->ctl->r20 == 0x3456);
  /* The real allocator fills fresh memory with a5; neither constructor
   * nor finisher writes this upper half. This is allocation history. */
  check("V17 control upper half preserved", (unsigned short)p->ctl->r22 == 0xa5a5);
 }
 _delete_data_rx_modem(&ctx);
 check("deleted receiver", ctx.vmi_b == 0 && ctx.modem_vmi == 0);
}
int main(void)
{
 history(0x18,0); history(0x30,0); history(0x48,1); history(0x60,1);
 history(0x91,2); history(0x92,2); history(0x61,2);
 printf("native fax RX: %d checks, %d failures; 7 create/reinit/delete histories\n",checks,failures);
 return failures != 0;
}
