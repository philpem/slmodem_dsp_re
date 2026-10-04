#!/usr/bin/env python3
"""Cross sparse status dispatch and an incoming default result lifetime."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 start,end,fn=d.function(source,'_handle_status')
 head=fn[:fn.index('{')+1]
 forms={
 'switch-return':head+'\n\tswitch (code) {\n\tcase 1: return 10;\n\tcase 2: return 11;\n\tcase 4: return 12;\n\tdefault: return status;\n\t}\n}',
 'switch-assignment':head+'\n\tswitch (code) {\n\tcase 1: status = 10; break;\n\tcase 2: status = 11; break;\n\tcase 4: status = 12; break;\n\t}\n\treturn status;\n}',
 'if-assignment':head+'\n\tif (code == 1)\n\t\tstatus = 10;\n\telse if (code == 2)\n\t\tstatus = 11;\n\telse if (code == 4)\n\t\tstatus = 12;\n\treturn status;\n}'}
 cells={'baseline':source}
 for label,body in forms.items():cells[label]=source[:start]+body+source[end:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.SOURCE_PATHS=('src/voice/voice.c',);d.OUT_NAME='gcc3-batch50-voice-status';d.variants=variants;d.main()
