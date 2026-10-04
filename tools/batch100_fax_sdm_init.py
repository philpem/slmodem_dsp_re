#!/usr/bin/env python3
"""SDMv27 initializer original not-two fallthrough."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V27_SDM.c',);d.OUT_NAME='batch100-fax-sdm-init'
def variants(path,s):
 a,z,f=d.function(s,'SDMv27_init')
 old='''if (cfg->nbits == 2) {
		sdm->mask = 3;
		sdm->notmask = (unsigned short)~3;
	} else {
		sdm->mask = 7;
		sdm->notmask = (unsigned short)~7;
	}'''
 new='''if (cfg->nbits != 2) {
		sdm->mask = 7;
		sdm->notmask = (unsigned short)~7;
	} else {
		sdm->mask = 3;
		sdm->notmask = (unsigned short)~3;
	}'''
 assert old in f
 return {'baseline':s,'not-two-fallthrough':s[:a]+f.replace(old,new)+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
