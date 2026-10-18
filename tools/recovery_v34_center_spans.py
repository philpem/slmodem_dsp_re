#!/usr/bin/env python3
"""Replay nine witnessed V34 center-adaptation ownership/span controls.

The original has two circular history spans, continuously incremented
coefficient pointers, per-lane history reads and a cached imaginary-history
base. These controls isolate those boundaries. The 317-byte control was the
first retained source; the witnessed imaginary-history-owner control is now
retained after reviewing its downstream scratch changes. Other controls remain
diagnostic. Use the archived pre-recovery Gentoo baseline:

  python3 tools/recovery_v34_center_spans.py --domain center-span-ownership \
      --baseline-dir build/center-span-baseline
  python3 tools/recovery_v34_center_spans.py --audit

The shared driver checks the complete baseline object, mandatory bug
reproduction, header identities, all emitted bodies and exact gains/losses.
"""
import hashlib
import json
import re
import sys
import playbook_small_patterns as d

def reread(text):
 text=text.replace('int dre, dim, tap;','int tap;')
 text=text.replace('\t\tdre = q->dly_re[j];\n\t\tdim = q->dly_im[j];\n\n','')
 text=re.sub(r'\bdre\b','q->dly_re[j]',text)
 return re.sub(r'\bdim\b','q->dly_im[j]',text)

def variants(path,source):
 a,z,fn=d.function(source,'V34EqualizerCenterAdapt')
 start=fn.index('\tfor (n = 0; n < V34_EQ_CENTRE_TAPS; n++) {')
 body=fn[fn.index('\t\tdre ='):fn.rindex('\n\t}')]
 split=fn[:start].replace('\tint n;','\tint first = q->cursor + V34_EQ_CENTRE_FIRST;\n\tint limit = first + V34_EQ_CENTRE_TAPS;\n\tint wrapped = 0;\n\tint j, k;')
 split+='\tif (first >= V34_EQ_TAPS) {\n\t\tfirst -= V34_EQ_TAPS;\n\t\tlimit -= V34_EQ_TAPS;\n\t} else if (limit >= V34_EQ_TAPS) {\n\t\twrapped = limit - V34_EQ_TAPS;\n\t\tlimit = V34_EQ_TAPS;\n\t}\n\n\tk = V34_EQ_CENTRE_FIRST;\n'
 for loop in ['for (j = first; j < limit; j++, k++)','for (j = 0; j < wrapped; j++, k++)']:
  split+='\t'+loop+' {\n\t\tint dre, dim, tap;\n\n'+body+'\n\t}\n'
 split+='}'
 cells={'baseline':source}
 for label,new in [('split-spans',split),('lane-rereads',reread(fn)),('split-spans-lane-rereads',reread(split))]:cells[label]=source[:a]+new+source[z:]
 for label,text in list(cells.items()):
  new_a,new_z,newfn=d.function(text,'V34EqualizerCenterAdapt')
  newfn=newfn.replace('\tint n;','\tint n;\n\tshort *re = q->re + V34_EQ_CENTRE_FIRST;\n\tshort *im = q->im + V34_EQ_CENTRE_FIRST;')
  if label.startswith('split'):
   newfn=newfn.replace('\tint j, k;','\tint j;\n\tshort *re = q->re + V34_EQ_CENTRE_FIRST;\n\tshort *im = q->im + V34_EQ_CENTRE_FIRST;')
   newfn=newfn.replace('\tk = V34_EQ_CENTRE_FIRST;\n','').replace('j++, k++','j++')
  else:
   newfn=newfn.replace('\t\tint k = V34_EQ_CENTRE_FIRST + n;\n','')
  newfn=newfn.replace('q->re[k] =','*re++ =').replace('q->im[k] =','*im++ =').replace('q->re[k]','*re').replace('q->im[k]','*im')
  cells[label+'-coeff-pointers']=text[:new_a]+newfn+text[new_z:]
 assert len(cells)==len(set(cells.values()))==8
 text=cells['split-spans-lane-rereads-coeff-pointers']
 a,z,fn=d.function(text,'V34EqualizerCenterAdapt')
 old='\tshort *im = q->im + V34_EQ_CENTRE_FIRST;'
 assert fn.count(old)==1
 fn=fn.replace(old,old+'\n\tshort *dly_im = q->dly_im;').replace('q->dly_im[j]','dly_im[j]')
 cells['imaginary-history-owner']=text[:a]+fn+text[z:]
 assert len(cells)==len(set(cells.values()))==9
 return cells


def audit():
 """Check the frozen experiment's losses, collateral and noncode identity."""
 from gcc3_value_carriers_audit import inspect
 from elftools.elf.elffile import ELFFile

 root = d.ROOT / 'build/recovery-v34-center-spans'
 result = json.loads((root / 'results.json').read_text())
 assert result['revision'] == '06ba1f713'
 assert 'DSPLIB_REPRODUCE_BUGS' in result['config']
 family = result['families']['v34filters']
 cells = family['cells']
 target, prefilter = 'V34EqualizerCenterAdapt', 'V34EchoPreFilter'
 four = {target, prefilter, 'V34ModulatorProcess', 'V34SetupModulator'}
 # These are measured whole-TU results, not acceptance criteria for source.
 expected = {
     'baseline': (set(), ['SIZE', 164]),
     'split-spans': (four, ['SIZE', 3]),
     'lane-rereads': ({target}, ['SIZE', 164]),
     'split-spans-lane-rereads': (four, ['SIZE', 3]),
     'baseline-coeff-pointers': ({target}, ['SIZE', 161]),
     'split-spans-coeff-pointers': (four, ['BYTES', 325]),
     'lane-rereads-coeff-pointers': ({target, prefilter}, ['SIZE', 158]),
     'split-spans-lane-rereads-coeff-pointers': ({target, prefilter}, ['SIZE', 30]),
     'imaginary-history-owner': (four, ['SIZE', 11]),
 }
 assert set(cells) == set(expected), 'experiment domain changed'
 base_path = root / 'v34filters/baseline/candidate.o'
 base = inspect(base_path)
 metadata = {key: value for key, value in base.items() if key != 'text_positions'}
 assert cells['baseline']['baseline_reproduced']
 rows = {}
 for label, (changed, verdict) in expected.items():
     cell = cells[label]
     path = root / 'v34filters' / label / 'candidate.o'
     observed = inspect(path)
     assert {k: v for k, v in observed.items() if k != 'text_positions'} == metadata, (label, 'noncode or metadata')
     assert len(cell['functions']) == len(cell['verdicts']) == 26
     assert set(cell.get('changed_bodies', [])) == changed, label
     assert cell.get('gains', []) == [], label
     losses = [prefilter] if prefilter in changed else []
     assert cell.get('losses', []) == losses, label
     assert cell['verdicts'][target] == verdict, label
     # Re-read the actual objects; the saved JSON is not itself the detector.
     actual_changed = {name for name in cell['functions']
                       if d.b.body(str(path), name) != d.b.body(str(base_path), name)}
     assert actual_changed == changed, label
     rows[label] = {'changed': sorted(changed), 'gains': [], 'losses': losses,
                    'target': verdict, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

 # Known-data difference: prove that the inspector does not silently mask data.
 with base_path.open('rb') as stream:
     elf = ELFFile(stream)
     section = elf.get_section_by_name('.rodata')
     assert section['sh_size'] > 0 and not section['sh_flags'] & 4
     offset = section['sh_offset']
 bad = root / 'changed-data-negative-control.o'
 raw = bytearray(base_path.read_bytes())
 raw[offset] ^= 1
 bad.write_bytes(raw)
 assert inspect(bad)['allocated'] != base['allocated'], 'data difference missed'
 report = {'cells': rows, 'body_verdicts': 234,
           'noncode_metadata_relocations_equal': True,
           'negative_controls': {'changed_data_detected': True},
           'production_source_retained': True,
           'retained_cell': 'imaginary-history-owner',
           'reviewed_exact_loss': [prefilter]}
 (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
 print('9 cells / 234 body verdicts: declared losses confirmed; noncode, metadata and relocations equal; 1/1 changed-data control detected')


if __name__ == '__main__':
 if sys.argv[1:] == ['--audit']:
  audit()
  sys.exit(0)
 d.REV='06ba1f713'
 d.OUT_NAME='recovery-v34-center-spans'
 d.SOURCE_PATHS=('src/pump/v34/v34filters.c',)
 d.DUMP_FLAGS=('-v','-save-temps','-da')
 d.variants=variants
 d.main()
