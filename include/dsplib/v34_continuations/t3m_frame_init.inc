
	f->obj = obj;
	f->m = (unsigned char *)obj;
	f->rx = &obj->receiver;
	f->progress = &obj->progress;
	f->mst = 0;
	f->nbits = 0;
