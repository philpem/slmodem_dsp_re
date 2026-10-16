#!/usr/bin/env python3
"""Cross independently witnessed FIFO consumer owners, tail width and input age."""
import sys
from itertools import product
import playbook_small_patterns as d
from recovery_fifo_capture import variants as opening

def variants(path, source):
    base=opening(path,source)["reads-1-write-1"]
    result={"baseline":source}
    for owner, tail, capture in product((False,True),repeat=3):
        text=base
        if owner:
            a,z,fn=d.function(text,"_tx_scrambled_ones_state")
            old="if (rd < ctx->tx_bytes_per_block)"
            assert fn.count(old)==1
            fn=fn.replace(old,"if (*tx_data_count < ctx->tx_bytes_per_block)")
            text=text[:a]+fn+text[z:]
        if tail:
            a,z,fn=d.function(text,"_tx_nulls_state")
            old="*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1;"
            assert fn.count(old)==1
            fn=fn.replace(old,"*word8 = (unsigned short)ctx->tx_fifo->size - ctx->tx_fifo->count - 1;")
            text=text[:a]+fn+text[z:]
        if capture:
            a,z,fn=d.function(text,"_tx_nulls_state")
            old="\t\tint n;"
            assert fn.count(old)==1
            fn=fn.replace(old,old+"\n\t\tconst unsigned char *input;")
            old="\t\tctx->state = CLASS1_TX_DATA_STATE;"
            assert fn.count(old)==1
            fn=fn.replace(old,"\t\tinput = (const unsigned char *)(long)word4;\n"+old)
            old="_handle_data_input(ctx, (const unsigned char *)(long)word4,"
            assert fn.count(old)==1
            fn=fn.replace(old,"_handle_data_input(ctx, input,")
            text=text[:a]+fn+text[z:]
        result["owner-%d-tail-%d-capture-%d"%(owner,tail,capture)]=text
    assert len(result)==len(set(result.values()))==9
    return result

if __name__=="__main__":
    assert "--domain" in sys.argv
    d.REV="4169cb3b"
    d.OUT_NAME="recovery-fifo-owners-v2"
    d.SOURCE_PATHS=("src/fax/cDATAtx.c",)
    d.DUMP_FLAGS=("-v","-save-temps","-dr","-dc")
    d.variants=variants
    d.main()
