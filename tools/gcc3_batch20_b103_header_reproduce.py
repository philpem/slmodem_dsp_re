#!/usr/bin/env python3
"""Cross the coordinated B103FP return declaration against stable wrapper source."""
import argparse,sys
from pathlib import Path
import playbook_small_patterns as driver

if __name__=='__main__':
    ap=argparse.ArgumentParser(add_help=False)
    ap.add_argument('--candidate-header',type=Path,required=True)
    opts,rest=ap.parse_known_args();sys.argv=sys.argv[:1]+rest
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    header=opts.candidate_header.read_text()
    original=(driver.ROOT/'include/dsplib/b103fp.h').read_text()
    expected=original.replace('short ModDataB103(', 'unsigned short ModDataB103(').replace('short TxNoCarrierB103(', 'unsigned short TxNoCarrierB103(')
    assert expected==header,'overlay must contain exactly the two coordinated return declarations'
    stable=(driver.ROOT/'build/gcc3-batch20-v90-b103-owner/b103/self-rate-owner-byte-print/b103.c').read_text()
    driver.REV='93d7eee1'
    driver.OUT_NAME='gcc3-batch20-v90-b103-header'
    driver.SOURCE_PATHS=('src/pump/b103/b103.c',)
    driver.variants=lambda path,source:{'baseline':source,'stable-original-header':stable,'stable-unsigned-return-header':stable}
    driver.HEADER_OVERLAYS=lambda path,label:{'dsplib/b103fp.h':header} if label=='stable-unsigned-return-header' else {}
    driver.main()
