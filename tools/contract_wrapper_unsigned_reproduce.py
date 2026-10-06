#!/usr/bin/env python3
"""Cross wrapper's original unsigned arithmetic and ring-span reload sites."""
import playbook_small_patterns as d

def variants(path, source):
    cells={}
    for unsigned in (False,True):
        for reload in (False,True):
            text=source
            if unsigned:
                assert text.count('imin(int a, int b)')==1
                text=text.replace('static int\nimin(int a, int b)','static unsigned int\nimin(unsigned int a, unsigned int b)')
                text=text.replace('int span = 2 * w->host_frag;', 'unsigned int span = 2U * w->host_frag;')
            if reload:
                span='unsigned int span = 2U * w->host_frag;' if unsigned else 'int span = 2 * w->host_frag;'
                text=text.replace('\t'+span+'\n','')
                text=text.replace('\t\tint n = imin(count, w->host_frag);','\t\t'+span+'\n\t\tint n = imin(count, w->host_frag);')
                factor='2U' if unsigned else '2'
                for field in ('in.wr','out.rd'):
                    old='w->'+field+' = (w->'+field+' + n) % span;'
                    assert text.count(old)==1
                    text=text.replace(old,'w->'+field+' = (w->'+field+' + n) % ('+factor+' * w->host_frag);')
            label='baseline' if not(unsigned or reload) else 'unsigned-%d-reload-%d'%(unsigned,reload)
            cells[label]=text
    return cells

if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/core/dp_wrapper.c',)
    d.OUT_NAME='contract-wrapper-unsigned';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants;d.main()
