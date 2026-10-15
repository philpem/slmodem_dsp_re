#!/usr/bin/env python3
"""Compare promoted ring requirement independently of the selected carrier."""
import playbook_small_patterns as d


def variants(path,source):
    start,end,fn=d.function(source,'FAXVMI_create')
    cells={}
    for wide_need in (False,True):
        for wide_ring in (False,True):
            label={(False,False):'baseline',(False,True):'ring',
                   (True,False):'need',(True,True):'both'}[wide_need,wide_ring]
            body=fn
            if wide_need or wide_ring:
                old='\tunsigned short need, ring;'
                assert body.count(old)==1
                body=body.replace(old,'\t'+('int' if wide_need else 'unsigned short')+' need;\n'
                                  '\t'+('int' if wide_ring else 'unsigned short')+' ring;')
            if wide_need:
                body=body.replace('need = (unsigned short)(vmi->max_frame + 3);','need = vmi->max_frame + 3;')
                body=body.replace('ring = (vmi->fifo_size >= need) ? vmi->fifo_size : need;',
                                  'ring = (vmi->fifo_size >= need) ? vmi->fifo_size : (unsigned short)need;')
            cells[label]=source[:start]+body+source[end:]
    assert len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    d.REV='1ec83de9'
    d.SOURCE_PATHS=('src/fax/faxvmi.c',)
    d.OUT_NAME='gcc3-vmi-ring-capacity'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
