#!/usr/bin/env python3
"""Replay declared DIL zero-trip/load and DImode product/add boundaries."""
import argparse,sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
    cells={}
    if path.endswith('/b103.c'):
        for debug,idbranch,loop,switch in itertools.product((False,True),repeat=4):
            text=source
            if debug:
                text=text.replace('#include <stddef.h>','#include <stddef.h>\n#include "dsplib/debug.h"')
                text=text.replace('\t(void)max_frag;','\t(void)max_frag;\n\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("b103: create...\\n");')
                marker='\tdp->last_status = 1;'
                text=text.replace(marker,'\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("b103: %s config %d,%d,%d,%d,%d %d",\n\t\t    cfg.v21 ? "V.21" : "Bell103", cfg.call_type, cfg.v21,\n\t\t    cfg.tone_timeout_ticks, cfg.f10, cfg.f14, cfg.tx_scale);\n\n'+marker)
                text=text.replace('\tif (result != self->last_status)\n\t\tself->last_status = result;','\tif (result != self->last_status) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("b103: B103 state -> %x (msg: %x)\\n",\n\t\t\t    result, result & 0xff);\n\t\tself->last_status = result;\n\t}')
                text=text.replace('\t} else if (status == 7) {','\t} else if (status == 7) {\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("b103: ReturnStatus = BELL_103_LINKED\\n");')
            if idbranch:text=text.replace('\tcfg.v21 = (id == DP_V21);','\tif (id == DP_B103)\n\t\tcfg.v21 = 0;\n\telse\n\t\tcfg.v21 = (id == DP_V21);')
            if loop:text=text.replace('for (i = (unsigned short)n_tx; i >= 0; i--)','for (i = (unsigned short)n_tx; --i >= 0; )')
            if switch:
                start=text.index('\tif (status == 0) {');end=text.index('\n\t/*',start)
                body='\tresult = DPSTAT_OK;\n\tswitch (status) {\n\tcase 0:\n\t\tself->tx_bits_wanted = B103_BITS_PER_BLOCK;\n\t\tbreak;\n\tcase 5:\n\tcase 6:\n\t\tself->tx_bits_wanted = 0;\n\t\tresult = DPSTAT_ERROR;\n\t\tbreak;\n\tcase 7:\n'
                if debug:body+='\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("b103: ReturnStatus = BELL_103_LINKED\\n");\n'
                body+='\t\tresult = DPSTAT_CONNECT;\n\t\tbreak;\n\tdefault:\n\t\tself->tx_bits_wanted = 0;\n\t\tbreak;\n\t}\n'
                text=text[:start]+body+text[end:]
            cells['-'.join(n for n,v in [('debug',debug),('id-branch',idbranch),('predecrement',loop),('status-switch',switch)] if v) or 'baseline']=text
    elif path.endswith('V90AutoDigitalImpDetector.cpp'):
        start,end,fn=driver.function(source,'V90AutoDigitalImpDetector::resetStudyUrefHandler')
        head=fn[:fn.index('\tif (qc == 0) {')]
        plain=fn[fn.index('\tif (qc == 0) {')+len('\tif (qc == 0) {\n'):fn.index('\n\t\tstudyState = 0;')]
        plain='\n'.join(line[1:] if line.startswith('\t') else line for line in plain.split('\n'))
        hot=fn[fn.index('\tinv = 1.0f / float_a950;'):fn.rindex('\n\tstudyState = 0;')]
        clear='\tstudyState = 0;\n\tstateSampleCount = 0;'
        indent=lambda body:'\n'.join('\t'+line if line else line for line in body.split('\n'))
        cells={'baseline':source}
        for label,first,second,cond in [('plain-first-shared',plain,hot,'qc == 0'),('hot-first-shared',hot,plain,'qc != 0')]:
            body=head+'\tif ('+cond+') {\n'+indent(first)+'\n\t} else {\n'+indent(second)+'\n\t}\n\n'+clear+'\n}'
            cells[label]=source[:start]+body+source[end:]
        body=head+'\tif (qc != 0) {\n'+indent(hot)+'\n\n'+indent(clear)+'\n\t\treturn;\n\t}\n\n'+plain+'\n\n'+clear+'\n}'
        cells['hot-return']=source[:start]+body+source[end:]
    elif path.endswith('V90CPpck.cpp'):
        start,end,fn=driver.function(source,'float2Bits')
        for switch,invert in itertools.product((False,True),repeat=2):
            body=fn
            if switch:
                body=body.replace('\tif (mode == 0) {','\tswitch (mode) {\n\tcase 0:')
                body=body.replace('\t} else if (mode == 1) {','\t\tbreak;\n\tcase 1:')
                assert body.endswith('\n\t}\n}')
                body=body[:-5]+'\n\t\tbreak;\n\t}\n}'
            if invert:
                for table,idx in [('fltTable2',15),('fltTable1',6)]:
                    old='\t\t\tif (%s[i] > x) {\n\t\t\t\tbits[%d - i] = 0;\n\t\t\t} else {\n\t\t\t\tbits[%d - i] = 1;\n\t\t\t\tx -= %s[i];\n\t\t\t}'%(table,idx,idx,table)
                    new='\t\t\tif (!(%s[i] > x)) {\n\t\t\t\tbits[%d - i] = 1;\n\t\t\t\tx -= %s[i];\n\t\t\t} else {\n\t\t\t\tbits[%d - i] = 0;\n\t\t\t}'%(table,idx,table,idx)
                    assert body.count(old)==1;body=body.replace(old,new)
            cells['-'.join(n for n,v in [('switch',switch),('subtraction-first',invert)] if v) or 'baseline']=source[:start]+body+source[end:]
    elif path.endswith('V90Parameters.cpp'):
        for store,fabs in itertools.product((False,True),repeat=2):
            text=source
            if store:
                marker='\t\tfloat pr = (float)(int)(tempPR / 5) * 0.5f;'
                assert text.count(marker)==1
                text=text.replace('\t\tDIGITAL_POWER_REDUCTION = pr;\n','')
                text=text.replace(marker,marker+'\n\t\tDIGITAL_POWER_REDUCTION = pr;')
            if fabs:
                text = "#include <math.h>\n" + text
                text=text.replace('\t\tint whole = (int)pr;','\t\tint whole = (int)fabsf(pr);')
                text=text.replace('\t\tif (whole < 0)\n\t\t\twhole = -whole;\n','')
            cells['-'.join(n for n,v in [('early-field-store',store),('float-abs',fabs)] if v) or 'baseline']=text
    elif path.endswith('V90ModulusDecoder.cpp'):
        initial='\tacc = (long long)in[5] * constellationSize4 + in[4];'
        for first,rest in itertools.product((False,True),repeat=2):
            text=source
            if first:text=text.replace(initial,'\tacc = in[5];\n\tacc *= constellationSize4;\n\tacc += in[4];')
            if rest:
                for i in range(4):
                    old='\tacc = acc * constellationSize%d + in[%d];'%(i,i)
                    assert text.count(old)==1
                    text=text.replace(old,'\tacc *= constellationSize%d;\n\tacc += in[%d];'%(i,i))
            cells['-'.join(n for n,v in [('split-initial',first),('split-rest',rest)] if v) or 'baseline']=text
    else:
        for zero,code,late in itertools.product((False,True),repeat=3):
            text=source
            if zero:
                for count in ('n','count'):
                    old='\tif (%s == 0)\n\t\treturn length;\n'%count
                    assert text.count(old)==1;text=text.replace(old,'')
            if code:
                for old,marker in [('unsigned int code = TO[type][i];','\t\tint segBounds[2][8] = {'),('unsigned int code = dil->dilCode[i];','\t\tint codeSegmentsBoundries[2][8] = {')]:
                    assert text.count('\t\t'+old+'\n')==1
                    text=text.replace('\t\t'+old+'\n','');text=text.replace(marker,'\t\t'+old+'\n'+marker)
            if late:
                for guard in ('if ((int)type > DIL_TYPE_ADI_QC)','if (dil == 0)'):
                    pos=text.index('calculateDilLength(', text.index('calculateDilLength(')+1) if guard=='if (dil == 0)' else text.index('calculateDilLength(DilType type')
                    tail=text[pos:]
                    assert tail.count('\tunsigned int length = 0;')>=1
                    tail=tail.replace('\tunsigned int length = 0;','\tunsigned int length;',1)
                    tail=tail.replace('\t'+guard+'\n\t\treturn length;','\t'+guard+'\n\t\treturn 0;\n\n\tlength = 0;',1)
                    text=text[:pos]+tail
            cells['-'.join(n for n,v in [('zero-trip',zero),('early-code',code),('late-result',late)] if v) or 'baseline']=text
    if path.endswith('/b103.c') and B103_NEXT:
        combined=cells['debug-id-branch-predecrement-status-switch']
        cells={'baseline':source}
        for owner,slots,eager in itertools.product((False,True),repeat=3):
            text=combined
            if owner:
                old='\tif (id == DP_B103)\n\t\tcfg.v21 = 0;\n\telse\n\t\tcfg.v21 = (id == DP_V21);'
                new='\t{\n\t\tint v21;\n\t\tif (id == DP_B103)\n\t\t\tv21 = 0;\n\t\telse\n\t\t\tv21 = (id == DP_V21);\n\t\tcfg.v21 = v21;\n\t}'
                assert text.count(old)==1;text=text.replace(old,new)
            if slots:text=text.replace('\tshort n_tx;\n\tshort n_rx;','\tshort n_rx;\n\tshort n_tx;')
            if eager:text=text.replace('if ((unsigned)result != dp->status && result == DPSTAT_CONNECT)','if (((unsigned)result != dp->status) & (result == DPSTAT_CONNECT))')
            cells['-'.join(n for n,v in [('id-local',owner),('rx-before-tx',slots),('eager-edge',eager)] if v) or 'combined']=text
    if path.endswith('/b103.c') and B103_ORDER:
        combined=cells['debug-id-branch-predecrement-status-switch']
        def ordered(text):
            spans=[]
            for name in ('dp_b103_init','b103_create','dp_b103_exit','b103_delete','b103_process'):
                start,end,fn=driver.function(text,name)
                type_start=text.rfind('\n',0,start-1)+1
                # b103_create return type is on its own previous line.
                spans.append((type_start,end,name,text[type_start:end]))
            assert [n for _,_,n,_ in sorted(spans)]==['dp_b103_init','b103_create','dp_b103_exit','b103_delete','b103_process']
            pieces={n:body for _,_,n,body in spans}
            first=min(t[0] for t in spans)
            # Remove definitions only, retaining explanatory comments in place.
            for a,e,n,body in sorted(spans,reverse=True):text=text[:a]+text[e:]
            return text[:first]+'\n\n'.join(pieces[n] for n in ('b103_create','b103_delete','b103_process','dp_b103_init','dp_b103_exit'))+'\n'+text[first:]
        cells={'baseline':source,'combined':combined,'object-order':ordered(source),'combined-object-order':ordered(combined)}
    if path.endswith('/b103.c') and B103_TYPES:
        combined=cells['combined-object-order']
        cells={'baseline':source}
        for ternary,counts,result in itertools.product((False,True),repeat=3):
            text=combined
            if ternary:text=text.replace('cfg.v21 ? "V.21" : "Bell103"','cfg.v21 == 0 ? "Bell103" : "V.21"')
            if counts:text=text.replace('\tshort n_tx;\n\tshort n_rx;','\tunsigned short n_tx;\n\tunsigned short n_rx;')
            if result:text=text.replace('\tint result;','\tunsigned int result;')
            cells['-'.join(n for n,v in [('zero-name',ternary),('unsigned-counts',counts),('unsigned-result',result)] if v) or 'combined']=text
    if path.endswith('/b103.c') and B103_OWNER:
        combined=cells['zero-name']
        cells={'baseline':source}
        for owner,narrow in itertools.product((False,True),repeat=2):
            text=combined
            if owner:text=text.replace('modem_set_param(dp->modem,','modem_set_param(self->dp.modem,')
            if narrow:text=text.replace('result, result & 0xff);','result, (unsigned char)result);')
            cells['-'.join(n for n,v in [('self-rate-owner',owner),('byte-print',narrow)] if v) or 'combined']=text
    if path.endswith('/b103.c') and B103_PARAMETER:
        combined=cells['self-rate-owner-byte-print']
        cells={'baseline':source,'combined':combined}
        for param in ('typed','cast'):
            for addresses in (False,True):
                text=combined
                assert text.count('\tstruct dp *dp = (struct dp *)dp_arg;')==1
                text=text.replace('\tstruct dp *dp = (struct dp *)dp_arg;\n','')
                if param=='typed':
                    text=text.replace('b103_process(void *dp,','b103_process(struct dp *dp,').replace('b103_process(void *dp_arg,','b103_process(struct dp *dp,')
                else:
                    pos=text.index('b103_process(void *dp_arg,')
                    end=text.index('\n}\n',pos)+2
                    fn=text[pos:end].replace('dp->','((struct dp *)dp_arg)->')
                    fn=fn.replace('self->((struct dp *)dp_arg)->','self->dp.')
                    # self->dp is a struct member, only local dp pointer accesses change.
                    text=text[:pos]+fn+text[end:]
                if addresses:
                    marker='\tn_rx = B103_DP_FRAG;'
                    text=text.replace(marker,marker+'\n\t{\n\t\tshort *tx_count = &n_tx;\n\t\tshort *rx_count = &n_rx;')
                    text=text.replace('self->rx_bits, &n_tx, &n_rx);','self->rx_bits, tx_count, rx_count);\n\t}')
                cells[param+('-count-addresses' if addresses else '')]=text
    if path.endswith('/b103.c') and B103_RESULT:
        combined=cells['self-rate-owner-byte-print']
        cells={'baseline':source}
        for separate,early in itertools.product((False,True),repeat=2):
            text=combined
            if separate:
                marker='\tint result;'
                assert text.count(marker)==1;text=text.replace(marker,marker+'\n\tint fp_status;')
                text=text.replace('\tresult = B103FP_modem(','\tfp_status = B103FP_modem(')
                begin=text.index('\tif (result != self->last_status)');end=text.index('\tresult = DPSTAT_OK;',begin)
                block=text[begin:end].replace('result','fp_status')
                text=text[:begin]+block+text[end:]
            if early:
                assignment='\tresult = DPSTAT_OK;\n'
                assert text.count(assignment)==1
                if separate:text=text.replace(assignment,'')
                marker='\tn_rx = B103_DP_FRAG;'
                text=text.replace(marker,assignment+marker)
            cells['-'.join(n for n,v in [('separate-status',separate),('early-dp-result',early)] if v) or 'combined']=text
    assert len(cells)==len(set(cells.values()))
    return cells

if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False);parser.add_argument('--b103-result',action='store_true');parser.add_argument('--b103-parameter',action='store_true');parser.add_argument('--b103-owner',action='store_true');parser.add_argument('--b103-types',action='store_true');parser.add_argument('--b103-order',action='store_true');parser.add_argument('--b103-next',action='store_true');parser.add_argument('--family',choices=('small','cppck','parameters','adid','b103'),default='small')
    opts,rest=parser.parse_known_args();sys.argv=sys.argv[:1]+rest;B103_NEXT=opts.b103_next;B103_ORDER=opts.b103_order or opts.b103_types or opts.b103_owner or opts.b103_parameter or opts.b103_result;B103_TYPES=opts.b103_types or opts.b103_owner or opts.b103_parameter or opts.b103_result;B103_OWNER=opts.b103_owner or opts.b103_parameter or opts.b103_result;B103_PARAMETER=opts.b103_parameter;B103_RESULT=opts.b103_result
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-v90-'+opts.family+('-next' if B103_NEXT else '')+('-result' if B103_RESULT else '-parameter' if B103_PARAMETER else '-owner' if B103_OWNER else '-types' if B103_TYPES else '-order' if B103_ORDER else '')
    driver.SOURCE_PATHS={'small':('src/pump/v90/V90DilDescriptorSettings.cpp','src/pump/v90/V90ModulusDecoder.cpp'),'cppck':('src/pump/v90/V90CPpck.cpp',),'parameters':('src/pump/v90/V90Parameters.cpp',),'adid':('src/pump/v90/V90AutoDigitalImpDetector.cpp',),'b103':('src/pump/b103/b103.c',)}[opts.family]
    if opts.family=='adid': driver.DUMP_FLAGS=('-dr',)
    driver.variants=variants;driver.main()
