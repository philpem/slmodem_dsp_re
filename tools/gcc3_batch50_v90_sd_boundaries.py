#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/pump/v90/V90SdDetector.cpp',);d.OUT_NAME='gcc3-batch50-v90-sd-boundaries'
def variants(path,source):
 cells={'baseline':source}
 for ptr,late,common in itertools.product((False,True),repeat=3):
  if not any((ptr,late,common)):continue
  start,end,fn=d.function(source,'V90SdDetector::process')
  if ptr:
   fn=fn.replace('\tdo {\n\t\thistory[i] = history[i - 1];\n\t} while (--i != 0);','\tfloat *previous = history + historyLength - 2;\n\tfloat *destination = history + historyLength - 1;\n\tdo {\n\t\t*destination = *previous;\n\t\tprevious--;\n\t\tdestination--;\n\t} while (--i != 0);')
  if late:
   fn=fn.replace('\tlong double correlation = 0.0L;','\tlong double correlation;').replace('\tlong double energy = 0.0L;','\tlong double energy;').replace('\thistory[0] = sample;','\thistory[0] = sample;\n\tcorrelation = 0.0L;\n\tenergy = 0.0L;')
  if common:
   fn=fn.replace('\tunsigned int i = historyLength - 1;','\tint result = 0;\n\tunsigned int i = historyLength - 1;')
   begin=fn.index('\tif (thresh_08 > energy)')
   comment=fn[fn.index('\t/*',begin):fn.index('\tif (!(thresh_0c >= ratio))',begin)]
   fn=fn[:begin]+'''\tif (thresh_08 > energy) {
		count = 0;
		result = 0;
	} else {
		ratio = correlation / energy;
'''+comment+'''		if (!(thresh_0c >= ratio)) {
			run = count + 1;
			count = run;
			if (run >= limit) result = 1;
		} else {
			result = -1;
			if (!(value_10 > ratio)) {
				count = 0;
				result = 0;
			}
		}
	}
	return result;
}'''
  label='-'.join(n for n,v in [('endpoints',ptr),('late-accumulators',late),('common-verdict',common)] if v)
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
