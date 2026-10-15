#!/usr/bin/env python3
"""Current partial adoption review of two original medium-fax read boundaries."""
import playbook_small_patterns as d
from batch50_fax_medium_reads import variants


if __name__=='__main__':
    d.REV='18f4ad68'
    d.SOURCE_PATHS=('src/fax/cDATAtx.c','src/fax/faxvmi_hdlc.c')
    d.OUT_NAME='remaining-medium-boundary'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
