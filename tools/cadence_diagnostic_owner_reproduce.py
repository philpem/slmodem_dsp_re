#!/usr/bin/env python3
"""Distinguish working diagnostic type from late descriptor name publication."""
import playbook_small_patterns as d
from service_constructor_fidelity_reproduce import variants as previous


def variants(path, source):
    fixed=previous(path,source)['original-switch-map-interior-on']
    a,z,fn=d.function(fixed,'cadence_create')
    begin=fn.index('\t} else {\n\t\t/* DIAL,')
    insertion=begin+len('\t} else {\n')
    work=fn[:insertion]+'\t\ttone = CADENCE_TONE_DIAL;\n'+fn[insertion:]
    old='dsplibs_debug_printf("TYPE %s\\n", c->name);'
    assert work.count(old)==1
    work=work.replace(old,'dsplibs_debug_printf("TYPE %s\\n", CadenceNames[tone]);')
    cells={'baseline':fixed,'working-tone':fixed[:a]+work+fixed[z:]}
    first=work.index('\tname = s->tone;')
    last=work.index('\tc->name = CadenceNames[name];',first)+len('\tc->name = CadenceNames[name];')
    block=work[first:last]
    late=work[:first]+work[last:]
    insert=late.index('\tcfg.a = c->sel_a;')
    late=late[:insert]+block+'\n\n'+late[insert:]
    cells['working-tone-late-name']=fixed[:a]+late+fixed[z:]
    extra='\tif (extra > 0)\n\t\tc->continuous = 0;\n\n\tc->int_2a4 = s->w6;'
    assert work.count(extra)==1
    full=work.replace(extra,'')
    assert full.count(block)==1
    full=full.replace(block,'')
    insert=full.index('\tcfg.a = c->sel_a;')
    full=full[:insert]+extra+'\n\n'+block+'\n\n'+full[insert:]
    cells['working-tone-late-descriptor']=fixed[:a]+full+fixed[z:]
    return cells


if __name__=='__main__':
    d.REV='18f4ad68'
    d.SOURCE_PATHS=('src/callprog/Cadence.c',)
    d.OUT_NAME='cadence-diagnostic-owner'
    d.DUMP_FLAGS=('-v','-save-temps','-da')
    d.variants=variants
    d.main()
