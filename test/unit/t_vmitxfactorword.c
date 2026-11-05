/* Synthetic writable-template override at the VMI constructor component
 * boundary. This does not instantiate a pump or claim service reachability. */
#include <string.h>
#include "harness.h"
#include "dsplib/faxcfg.h"
#include "dsplib/v17fax.h"
#include "dsplib/v29data.h"
#include "dsplib/sysdep.h"
extern int init_vmi_v17tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern int ref_init_vmi_v17tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern struct v17tx_cfg ref_V17TX_CFG;
static void
probe17(void)
{
	static const unsigned high_words[]={0,0x13570000u};
	int saved_ours=V17TX_CFG.fifo_size_factor;
	int saved_ref=ref_V17TX_CFG.fifo_size_factor;
	unsigned i;

	for(i=0;i<sizeof(high_words)/sizeof(high_words[0]);i++) {
		struct faxvmi_cfg a,b;
		struct v17tx_cfg *ca,*cb;
		int ra,rb;
		unsigned expected=high_words[i]|1u;
		V17TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ours&0xffff));
		ref_V17TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ref&0xffff));
		memset(&a,0x5a,sizeof a);memset(&b,0x5a,sizeof b);
		rb=ref_init_vmi_v17tx(&b,14400,0,0);ra=init_vmi_v17tx(&a,14400,0,0);
		V17TX_CFG.fifo_size_factor=saved_ours;
		ref_V17TX_CFG.fifo_size_factor=saved_ref;
		ca=(struct v17tx_cfg*)a.modem_cfg;cb=(struct v17tx_cfg*)b.modem_cfg;
		diff_eq_int("both configs allocated (%ld)",ca!=0&&cb!=0,1,i);
		if(!ca||!cb)continue;
		diff_eq_int("constructor bitrate (%ld)",ra,rb,i);
		diff_eq_int("native partial override (%ld)",(unsigned)cb->fifo_size_factor,expected,i);
		diff_eq_int("complete factor after override (%ld)",ca->fifo_size_factor,cb->fifo_size_factor,i);
		diff_eq_obj("complete copied TX config",struct v17tx_cfg,ca,cb,i);
		sysdep_free(ca);sysdep_free(cb);
	}
	V17TX_CFG.fifo_size_factor=saved_ours;
	ref_V17TX_CFG.fifo_size_factor=saved_ref;

}

extern int init_vmi_v29tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern int ref_init_vmi_v29tx(struct faxvmi_cfg *,unsigned short,int,void *);
extern struct v29tx_cfg ref_V29TX_CFG;
static void
probe29(void)
{
	static const unsigned high_words[]={0,0x13570000u};
	int saved_ours=V29TX_CFG.fifo_size_factor;
	int saved_ref=ref_V29TX_CFG.fifo_size_factor;
	unsigned i;

	for(i=0;i<sizeof(high_words)/sizeof(high_words[0]);i++) {
		struct faxvmi_cfg a,b;
		struct v29tx_cfg *ca,*cb;
		int ra,rb;
		unsigned expected=high_words[i]|2u;
		V29TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ours&0xffff));
		ref_V29TX_CFG.fifo_size_factor=(int)(high_words[i]|(unsigned)(saved_ref&0xffff));
		memset(&a,0x5a,sizeof a);memset(&b,0x5a,sizeof b);
		rb=ref_init_vmi_v29tx(&b,9600,0,0);ra=init_vmi_v29tx(&a,9600,0,0);
		V29TX_CFG.fifo_size_factor=saved_ours;
		ref_V29TX_CFG.fifo_size_factor=saved_ref;
		ca=(struct v29tx_cfg*)a.modem_cfg;cb=(struct v29tx_cfg*)b.modem_cfg;
		diff_eq_int("both configs allocated (%ld)",ca!=0&&cb!=0,1,i);
		if(!ca||!cb)continue;
		diff_eq_int("constructor bitrate (%ld)",ra,rb,i);
		diff_eq_int("native partial override (%ld)",(unsigned)cb->fifo_size_factor,expected,i);
		diff_eq_int("complete factor after override (%ld)",ca->fifo_size_factor,cb->fifo_size_factor,i);
		diff_eq_obj("complete copied TX config",struct v29tx_cfg,ca,cb,i);
		sysdep_free(ca);sysdep_free(cb);
	}
	V29TX_CFG.fifo_size_factor=saved_ours;
	ref_V29TX_CFG.fifo_size_factor=saved_ref;

}

int
main(void)
{
	diff_begin("V17/V29 TX synthetic template upper word survives VMI override");
	probe17();
	probe29();
	return diff_end();
}
