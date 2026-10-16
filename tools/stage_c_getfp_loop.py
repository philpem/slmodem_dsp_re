#!/usr/bin/env python3
"""Recover GetFP_Value's original repeated short-count recurrence."""
import playbook_small_patterns as d
d.REV='acfa905a';d.SOURCE_PATHS=('src/dsp/FP_math.c',);d.OUT_NAME='stage-c-getfp';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 a=s.index('short\nGetFP_Value(');z=s.index('\n}\n',a)+2;cells={'baseline':s}
 for label,loop in [('subtract','\twhile (remaining > 0) {\n\t\tremaining -= vb;\n\t\tcount++;\n\t}\n'),('negative-add','\tif (remaining > 0) {\n\t\tint step = -vb;\n\n\t\tdo {\n\t\t\tremaining += step;\n\t\t\tcount++;\n\t\t} while (remaining > 0);\n\t}\n')]:
  f='short\nGetFP_Value(short a, short b)\n{\n\tint va = a < 0 ? -a : a;\n\tint vb = b < 0 ? -b : b;\n\tint remaining = va << 14;\n\tshort count = 0;\n\n'+loop+'\n\tif ((int)a * (int)b > 0)\n\t\treturn count;\n\treturn -count;\n}'
  cells[label]=s[:a]+f+s[z:]
 assert len(cells)==len(set(cells.values()))==3
 return cells
d.variants=variants
if __name__ == '__main__':d.main()
