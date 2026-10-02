#!/usr/bin/env python3
"""Observed child owner reloads across B103 deletion calls."""
import playbook_small_patterns as driver


def variants(path,source):
    start,end,body=driver.function(source,'B103FP_delete')
    results={}
    for direct_dsp in (False,True):
        for direct_hdx in (False,True):
            text=body
            for owner,direct in (('dsp',direct_dsp),('hdx',direct_hdx)):
                if not direct:continue
                lines=text.splitlines(keepends=True)
                declarations=[line for line in lines if ' *'+owner+' = fp->'+owner+';' in line or ' *'+owner+';' in line]
                assert len(declarations)==1,(owner,declarations)
                text=text.replace(declarations[0],'')
                assignment='\t'+owner+' = fp->'+owner+';\n'
                if assignment in text:
                    assert text.count(assignment)==1
                    text=text.replace(assignment,'')
                text=text.replace(owner+'->','fp->'+owner+'->')
                text=text.replace('sysdep_free('+owner+');','sysdep_free(fp->'+owner+');')
            label=('-'.join(n for n,on in (('dsp',direct_dsp),('hdx',direct_hdx)) if on) or 'baseline')
            results[label]=source[:start]+text+source[end:]
    assert len(set(results.values()))==4
    return results


if __name__=='__main__':
    driver.REV='71a7aa1a'
    driver.OUT_NAME='playbook-b103-delete-lifetimes'
    driver.SOURCE_PATHS=('src/pump/b103/B103prc.c',)
    driver.variants=variants
    driver.main()
