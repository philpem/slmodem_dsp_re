#!/usr/bin/env python3
"""Original carrier threshold valid arm fallthrough."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17r_int.c',);d.OUT_NAME='batch100-fax-carrier-arm'
def variants(path,s):
 a,z,f=d.function(s,'CarrierDetectV17');old='''if (rx->fse.mse > V17RXS_DEC_ERROR_MAX)
			r = 0;
		else
			r &= 1;''';new='''if (rx->fse.mse <= V17RXS_DEC_ERROR_MAX)
			r &= 1;
		else
			r = 0;''';assert old in f
 return {'baseline':s,'valid-threshold-fallthrough':s[:a]+f.replace(old,new)+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
