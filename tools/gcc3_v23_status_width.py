#!/usr/bin/env python3
"""Cross original V23 diagnostic ownership with complete-EAX return ABI."""
import subprocess
import playbook_small_patterns as d
from gcc3_missing_debug_boundary import variants as diagnostic_variants


def variants(path, source):
    result = {}
    for label in ('baseline','diagnostics','promoted-status','both'):
        text = source
        if label in ('diagnostics','both') and path == 'src/pump/v23/v23.c':
            text = diagnostic_variants(path,source)['both-debug']
        if label in ('promoted-status','both'):
            for name in ('V23ModemMain','BwChDem_Progress'):
                text = text.replace('short\n'+name+'(', 'int\n'+name+'(')
        result[label] = text
    return result


def overlays(path, label):
    if label not in ('promoted-status','both'):
        return {}
    text = subprocess.check_output(['git','show','861901c2:include/dsplib/v23fp.h'],cwd=d.ROOT,text=True)
    for name in ('V23ModemMain','BwChDem_Progress'):
        assert text.count('short '+name+'(') == 1
        text = text.replace('short '+name+'(', 'int '+name+'(')
    return {'dsplib/v23fp.h':text}


if __name__ == '__main__':
    d.REV = '861901c2'
    d.SOURCE_PATHS = ('src/pump/v23/v23.c','src/pump/v23/v23modem.c',
                      'src/pump/v23/bwchdem.c','src/pump/v23/v23rx.c',
                      'src/pump/v23/v23tx.c','src/pump/v23/V23filt.c',
                      'src/core/dp_init.c','src/service/Rxcid.c')
    d.OUT_NAME = 'gcc3-v23-status-width'
    d.DUMP_FLAGS = ('-v','-save-temps','-da')
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
