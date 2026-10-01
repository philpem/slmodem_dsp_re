#!/usr/bin/env python3
"""Nine finite CID, calling-tone and silence source controls on complete period TUs."""
import playbook_small_patterns as driver

def variants(path,source):
    cells={'baseline':source}
    if path.endswith('/cid.c'):
        start,end,fn=driver.function(source,'cid_get_strings')
        old='\tif (ctx->mode != CID_MODE_FSK && ctx->mode != CID_MODE_FSK_DONE) {'
        assert fn.count(old)==1 and fn.count('out[i] = ctx->dtmf->digits[i];')==1
        for name,cache,direct in [('receiver',True,False),('destination',False,True),('both',True,True)]:
            body=fn
            if cache:body=body.replace(old,old+'\n\t\tstruct dtmf_rx *dtmf = ctx->dtmf;').replace('ctx->dtmf->digits[i]','dtmf->digits[i]')
            if direct:body=body.replace('out[i] =','ctx->strings[i] =')
            cells[name]=source[:start]+body+source[end:]
        assert len(cells)==len(set(cells.values()))==4
    elif path.endswith('/CallingTone.c'):
        start,end,fn=driver.function(source,'GenerateCallingTone')
        assert fn.count('\t\t\t\tshort v;')==1 and fn.count('ct->amplitude * v')==1
        for label,ctype in [('amplitude-short','short'),('amplitude-int','int')]:
            body=fn.replace('\t\t\t\tshort v;', '\t\t\t\tshort v;\n\t\t\t\t'+ctype+' amplitude = ct->amplitude;')
            body=body.replace('ct->amplitude * v','amplitude * v')
            cells[label]=source[:start]+body+source[end:]
        assert len(cells)==len(set(cells.values()))==3
    else:
        start,end,fn=driver.function(source,'silence_is_more_then')
        old='\treturn s->count > (int)(10.0f * t);';assert fn.count(old)==1
        body=fn.replace(old,'\tint threshold = (int)(10.0f * t);\n\n\treturn s->count > threshold;')
        cells['threshold']=source[:start]+body+source[end:]
        assert len(cells)==len(set(cells.values()))==2
    return cells

if __name__=='__main__':
    driver.REV='5632087b'
    driver.OUT_NAME='playbook-reserve-parent'
    driver.SOURCE_PATHS=('src/service/cidcore/cid.c','src/callprog/CallingTone.c','src/service/silence.c')
    driver.variants=variants
    driver.main()
