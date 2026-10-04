#!/usr/bin/env python3
"""Test voice command's observed common initialized result across debug calls."""
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'voice_dle_command')
 common=fn.replace('{\n\tswitch (cmd)', '{\n\tint status = 0;\n\tswitch (cmd)')
 common=common.replace('\t\treturn 0;','\t\tbreak;').replace('\t\treturn VOICE_DLE_CAN_STATUS;','\t\tstatus = VOICE_DLE_CAN_STATUS;\n\t\tbreak;')
 old='\t}\n\n\tif (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf("Unknown command - %2x\\n", cmd);\n\treturn 0;'
 new='\tdefault:\n\t\tif (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf("Unknown command - %2x\\n", cmd);\n\t}\n\treturn status;'
 assert old in common;common=common.replace(old,new)
 cells['common-switch']=source[:start]+common+source[end:]
 head='''voice_dle_command(struct voice_ctx *v, signed char cmd)
{
\tint status = 0;
\tif (cmd == VOICE_DLE_ETX) {
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("voice dle command: ETX\\n");
\t\tv->dle_etx = 1;
\t} else if (cmd == VOICE_DLE_CAN) {
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("voice <CAN> command\\n");
\t\tv->dle_can = 1;
\t\tstatus = VOICE_DLE_CAN_STATUS;
\t} else if (DSPLIB_DEBUG_ON()) {
\t\tdsplibs_debug_printf("Unknown command - %2x\\n", cmd);
\t}
\treturn status;
}'''
 cells['common-if']=source[:start]+head+source[end:]
 assert len(set(cells.values()))==3
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/voice/voice.c',);d.OUT_NAME='gcc3-batch100-voice-dle';d.variants=variants;d.main()
