#!/usr/bin/env python3
"""Compare one-case cleanup switch with reconstructed equality guard."""
import playbook_small_patterns as d

def variants(path,source):
    start,end,fn=d.function(source,'_delete_data_rx_modem')
    marker='\tif (vmi->slot == VMI_SLOT_V17RX) {'
    assert fn.count(marker)==1
    fn=fn.replace(marker,'\tswitch (vmi->slot) {\n\tcase VMI_SLOT_V17RX: {')
    marker='\t\tvmi = ctx->modem_vmi;\n\t}\n\n\tsysdep_free(vmi->modem_cfg);'
    assert fn.count(marker)==1
    fn=fn.replace(marker,'\t\tvmi = ctx->modem_vmi;\n\t\tbreak;\n\t}\n\tdefault:\n\t\tbreak;\n\t}\n\n\tsysdep_free(vmi->modem_cfg);')
    return {'baseline':source,'slot-switch':source[:start]+fn+source[end:]}
if __name__=='__main__':
    d.REV='e0052eec';d.SOURCE_PATHS=('src/fax/class1rx.c',);d.OUT_NAME='fax-rx-delete-dispatch'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
