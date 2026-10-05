#!/usr/bin/env python3
"""Test the original fax adapter's modem-status passthrough."""
import re
import subprocess
import playbook_small_patterns as d

def names(path):
    mode=re.search(r'Vmi_v(\d+)',path).group(1)
    return ['v'+mode+direction+'_process' for direction in ['tx','rx']]

def variants(path,source):
    text=source
    for name in names(path):
        start,end,fn=d.function(text,name)
        assert text[start-5:start]=='void\n'
        fn=fn.replace('\n\tV','\n\tint status = V',1)
        assert fn.count('\t*count = 0;')==1
        fn=fn.replace('\t*count = 0;','\t*count = 0;\n\treturn status;')
        text=text[:start-5]+'int\n'+fn+text[end:]
    return {'baseline':source,'modem-status':text}

def overlay(path,label):
    if label=='baseline':return {}
    text=subprocess.check_output(['git','show',d.REV+':include/dsplib/faxadapt.h'],cwd=d.ROOT,text=True)
    for name in names(path):
        text,n=re.subn(r'\bvoid\s+'+name+r'\(', 'int '+name+'(', text)
        assert n==1,(name,n)
    return {'dsplib/faxadapt.h':text}
if __name__=='__main__':
    d.REV='240481e6';d.SOURCE_PATHS=tuple('src/fax/Vmi_v'+m+'.c' for m in ['17','21','27','29'])
    d.OUT_NAME='fax-adapter-result';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.HEADER_OVERLAYS=overlay;d.main()
