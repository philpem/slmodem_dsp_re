/* Synthetic writable-template override at the VMI constructor component
 * boundary. This does not instantiate a pump or claim service reachability. */
#include <string.h>
#include "harness.h"
#include "dsplib/faxcfg.h"
#include "dsplib/v27fax.h"
#include "dsplib/sysdep.h"
extern int init_vmi_v27tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern int ref_init_vmi_v27tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern struct v27tx_cfg ref_V27TX_CFG;
int
main(void)
{
	static const unsigned high_words[]={0,0x13570000u};
	int saved_ours=V27TX_CFG.fifo_size_factor;
	int saved_ref=ref_V27TX_CFG.fifo_size_factor;
	unsigned i;
	diff_begin("V27 TX synthetic template upper word survives VMI override");
	for(i=0;i<sizeof(high_words)/sizeof(high_words[0]);i++) {
		struct faxvmi_cfg a,b;
		struct v27tx_cfg *ca,*cb;
		int ra,rb;
		unsigned expected=high_words[i]|2u;
		V27TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ours&0xffff));
		ref_V27TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ref&0xffff));
		memset(&a,0x5a,sizeof a);memset(&b,0x5a,sizeof b);
		rb=ref_init_vmi_v27tx(&b,4800,0,0);ra=init_vmi_v27tx(&a,4800,0,0);
		V27TX_CFG.fifo_size_factor=saved_ours;
		ref_V27TX_CFG.fifo_size_factor=saved_ref;
		ca=(struct v27tx_cfg*)a.modem_cfg;cb=(struct v27tx_cfg*)b.modem_cfg;
		diff_eq_int("both configs allocated (%ld)",ca!=0&&cb!=0,1,i);
		if(!ca||!cb)continue;
		diff_eq_int("constructor bitrate (%ld)",ra,rb,i);
		diff_eq_int("native partial override (%ld)",(unsigned)cb->fifo_size_factor,expected,i);
		diff_eq_int("complete factor after override (%ld)",ca->fifo_size_factor,cb->fifo_size_factor,i);
		diff_eq_obj("complete copied TX config",struct v27tx_cfg,ca,cb,i);
		sysdep_free(ca);sysdep_free(cb);
	}
	V27TX_CFG.fifo_size_factor=saved_ours;
	ref_V27TX_CFG.fifo_size_factor=saved_ref;
	return diff_end();
}
