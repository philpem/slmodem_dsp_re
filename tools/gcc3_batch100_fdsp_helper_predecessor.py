#!/usr/bin/env python3
"""Cross an independently exact predecessor with existing conversion helpers."""
import playbook_small_patterns as d
from gcc3_batch100_fdsp_conversion_helpers import variants as helpers
from gcc3_batch100_dtmf_create_return import variants as dtmf
d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-fdsp-helper-predecessor'
def variants(path,s):
 both=helpers(path,s)['in-1-out-1'];owner=dtmf(path,s)['failure-common-owner-return']
 combined=helpers(path,owner)['in-1-out-1']
 return {'baseline':s,'helpers':both,'exact-dtmf-owner':owner,'composed':combined}
d.variants=variants
if __name__=='__main__':d.main()
