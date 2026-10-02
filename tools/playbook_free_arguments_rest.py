#!/usr/bin/env python3
"""Grouped receiver frees crossed with ECC and PPS ignored arguments."""
import itertools
import re
import playbook_small_patterns as driver

SOURCE_PATHS=('src/dsp/fpm_fse.c','src/dsp/fpm_sre.c','src/dsp/fpm_ecc.c','src/dsp/fpm_pps.c','src/pump/v32/V32.c','src/fax/V17rx.c','src/fax/V27rx.c','src/fax/V29rx.c','src/fax/V17tx.c','src/fax/V27tx.c','src/fax/V29tx.c')
CELLS={}
for rx,ecc,pps in itertools.product((False,True),repeat=3):
    label='-'.join(n for n,on in [('rx',rx),('ecc',ecc),('pps',pps)] if on) or 'baseline'
    CELLS[label]=(('FSE','SRE') if rx else ())+(('ECC',) if ecc else ())+(('PPS',) if pps else ())


def variants(path,source):
    results={}
    for label,families in CELLS.items():
        text=source
        for family in families:
            name='FPM_'+family+'_free'
            if path=='src/dsp/fpm_'+family.lower()+'.c':
                pattern=name+r'\(struct fpm_'+family.lower()+r' \*(\w+)\)\n\{'
                text,n=re.subn(pattern,lambda m:m.group(0).replace(')\n{',', int unused)\n{\n\t(void)unused;'),text)
                assert n==1
            else:
                positions=list(re.finditer(r'(?m)^[ \t]+'+name+r'\(',text))
                for match in reversed(positions):
                    depth=1;end=match.end()
                    while depth:
                        if text[end]=='(':depth+=1
                        elif text[end]==')':depth-=1
                        end+=1
                    text=text[:end-1]+', 1'+text[end-1:]
        results[label]=text
    assert len(results)==8
    return results


def overlays(path,label):
    result={}
    for family in CELLS[label]:
        rel='dsplib/fpm_'+family.lower()+'.h'
        text=(driver.ROOT/'include'/rel).read_text()
        pattern=r'void FPM_'+family+r'_free\(struct fpm_'+family.lower()+r' \*\w+\);'
        text,n=re.subn(pattern,lambda m:m.group(0).replace(');',', int unused);'),text)
        assert n==1
        result[rel]=text
    return result


if __name__=='__main__':
    driver.REV='71a7aa1a'
    driver.OUT_NAME='playbook-free-arguments-rest'
    driver.SOURCE_PATHS=SOURCE_PATHS
    driver.variants=variants
    driver.HEADER_OVERLAYS=overlays
    driver.main()
