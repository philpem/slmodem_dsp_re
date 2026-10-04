#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='gcc3-batch100-vpcm-probe-vector'
def variants(path,source):
 start,end,fn=d.function(source,'VPcmFloModem::getUinfoValue')
 old="\t\t\tif (use[i])\n\t\t\t\tL2[j++] = (float)((long double)decade * 10.0L\n\t\t\t\t\t\t  + 60.0L);"
 assert fn.count(old)==1
 scalar=fn.replace('\t\t\tfloat decade;','\t\t\tfloat decade;\n\t\t\tfloat level;').replace(old,"\t\t\tlevel = (float)((long double)decade * 10.0L + 60.0L);\n\t\t\tif (use[i])\n\t\t\t\tL2[j++] = level;")
 vector=fn.replace('\tdouble *probe;','\tfloat probe_levels[VPCM_PROBE_TONES];\n\tdouble *probe;')
 vector=vector.replace('\t\t\tdecade = (float)log10l((long double)scaled);','\t\t\tprobe_levels[i] = scaled;\n\t\t\tdecade = (float)log10l((long double)probe_levels[i]);')
 vector=vector.replace(old,"\t\t\tprobe_levels[i] = (float)((long double)decade * 10.0L + 60.0L);\n\t\t\tif (use[i])\n\t\t\t\tL2[j++] = probe_levels[i];")
 return {'baseline':source,'unconditional-level':source[:start]+scalar+source[end:],'staged-probe-vector':source[:start]+vector+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
