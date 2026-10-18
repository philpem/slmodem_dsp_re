
	struct t3m_frame f;

	{ struct t3m_frame *v34_init_frame = &f; struct v34_object *v34_init_obj = obj; { struct t3m_frame *f = v34_init_frame; struct v34_object *obj = v34_init_obj;
#include "dsplib/v34_continuations/t3m_frame_init.inc"
} }
	t3m_txblock(&f, obj->txstate);
