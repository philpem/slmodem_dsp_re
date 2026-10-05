#!/usr/bin/env python3
"""DTMF acceptance using independently witnessed conditional-mask updates."""
import playbook_small_patterns as d
import services_dtmf_predicate_reproduce as prior

def mask(t):
 old='''	ok = (elo * 7.94f >= ehi)
	    && (ehi * 2.82f >= elo)
	    && (max_lo * 0.96f >= elo)
	    && (max_hi * 0.96f >= ehi)
	    && (rest * 1.36f >= pair);'''
 assert t.count(old)==1
 new='\tok = 1;\n'
 for expr in ('elo * 7.94f >= ehi','ehi * 2.82f >= elo','max_lo * 0.96f >= elo','max_hi * 0.96f >= ehi','rest * 1.36f >= pair'):
  new+='\tif (!('+expr+'))\n\t\tok = 0;\n'
 return t.replace(old,new.rstrip())
def variants(path,source):
 p=prior.variants(path,source)
 return {'baseline':source,'early-control':p['eager-0-early-1'],'mask-late':mask(source),'mask-early':mask(p['eager-0-early-1'])}
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/service/Dtmf.c',);d.OUT_NAME='services-dtmf-mask'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
