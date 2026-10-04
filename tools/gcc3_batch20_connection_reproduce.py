#!/usr/bin/env python3
"""Replay finite evaluator verdict ownership and weighted-count load boundaries."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    for shared,early in itertools.product((False,True),repeat=2):
        text=source
        for name,counter in [('indicateLocalRetrain','nofV90Retrains'),('indicateRemoteRetrain','nofRemoteRetrains')]:
            start,end,fn=driver.function(text,'V90ConnectionEvaluator::'+name)
            declaration='\tint verdict = V90CE_VERDICT_RETRAIN;\n'
            marker='\t'+counter+'++;\n'
            if early:fn=fn.replace('{\n','{\n'+declaration,1)
            else:fn=fn.replace(marker,marker+declaration)
            threshold=fn.index(') {',fn.index('\tif ('))+len(') {')
            fn=fn[:threshold]+'\n\t\tverdict = V90CE_VERDICT_FALLBACK_V34;'+fn[threshold:]
            if shared:
                old='\t\tdataDurationCounter = 0;\n\t\tretrainCounter = 0;\n\t\treturn V90CE_VERDICT_FALLBACK_V34;\n'
                assert fn.count(old)==1;fn=fn.replace(old,'')
            else:fn=fn.replace('return V90CE_VERDICT_FALLBACK_V34;','return verdict;')
            fn=fn.replace('return V90CE_VERDICT_RETRAIN;','return verdict;')
            text=text[:start]+fn+text[end:]
        cells['retrain-'+('shared' if shared else 'duplicate')+'-cleanup-'+('entry-verdict' if early else 'post-counter-verdict')]=text
    start,end,fn=driver.function(source,'V90ConnectionEvaluator::updateAvePdsnr')
    for hot,member in itertools.product((False,True),repeat=2):
        if not hot and not member:continue
        body=fn
        if member:body=body.replace('(nofOldSymbols * avePdsnr','(avePdsnrNofSymbols * avePdsnr')
        if hot:
            pos=body.index('\tif (nofOldSymbols == 0) {');head=body[:pos]
            first=body.index('\n\t}',pos)
            cold=body[pos+len('\tif (nofOldSymbols == 0) {\n'):first].replace('\t\treturn;\n','')
            weighted=body[first+len('\n\t}\n'):body.rindex('\n}')].strip('\n')
            weighted='\n'.join('\t'+s if s else s for s in weighted.split('\n'))
            body=head+'\tif (nofOldSymbols != 0) {\n'+weighted+'\n\t} else {\n'+cold+'\n\t}\n}'
        cells['average-'+('-'.join(n for n,v in [('hot-first',hot),('member-product',member)] if v))]=source[:start]+body+source[end:]
    combo=cells['retrain-shared-cleanup-entry-verdict']
    a,e,new=driver.function(cells['average-hot-first-member-product'],'V90ConnectionEvaluator::updateAvePdsnr')
    start,end,old=driver.function(combo,'V90ConnectionEvaluator::updateAvePdsnr')
    cells['combined-three-gains']=combo[:start]+new+combo[end:]
    return cells

if __name__=='__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-connection'
    driver.SOURCE_PATHS=('src/pump/v90/V90ConnectionEvaluator.cpp',);driver.variants=variants;driver.main()
