#!/usr/bin/env python3
"""Replay two original-byte/relocation-backed bulk data repairs on full TUs."""
import playbook_small_patterns as d

def variants(path,source):
    if path.endswith('v34filters.c'):
        begin=source.index('static const short p2400[');end=source.index('\n};',begin)
        table=source[begin:end];assert table.count('-7786')==2
        fixed=source[:begin]+table.replace('-7786','-7790')+source[end:]
    else:
        old='const short MTDb103Org_CFG[6] = {\n\t0, 0, 2, 29820, 10, 0,\n};'
        assert source.count(old)==1
        fixed=source.replace(old,'const struct fpm_mtd_cfg MTDb103Org_CFG = {\n\tMTDb103_COEF, 2, 29820, 10, 0,\n};')
    return {'baseline':source,'original-data':fixed}

if __name__=='__main__':
    d.REV='013877f9'
    d.OUT_NAME='recovery-storage-content-fixes'
    d.SOURCE_PATHS=('src/pump/v34/v34filters.c','src/pump/b103/b103_tables.c')
    d.DUMP_FLAGS=()
    d.variants=variants
    d.main()
