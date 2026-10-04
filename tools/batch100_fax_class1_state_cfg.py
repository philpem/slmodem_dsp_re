#!/usr/bin/env python3
"""Two original small Class1 state CFG witnesses."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/class1.c',);d.OUT_NAME='batch100-fax-class1-state-cfg'
def variants(path,s):
 out={}
 for answer,idle in ((False,False),(True,False),(False,True),(True,True)):
  label='baseline'if not(answer or idle)else'-'.join(x for yes,x in((answer,'tone-fallthrough'),(idle,'literal-idle-arms'))if yes);x=s
  if answer:
   a,z,f=d.function(x,'_answer_tone_state')
   old='\tif (ctx->countdown > ctx->answer_tone_blocks) {\n\t\tcHDLCtx_preamble_state_init(ctx);\n\t\treturn 0;\n\t}\n\tFPM_TONE_generate(ctx->tone, tx, CLASS1_BLOCK_SAMPLES);\n\treturn 0;'
   new='\tif (ctx->countdown <= ctx->answer_tone_blocks) {\n\t\tFPM_TONE_generate(ctx->tone, tx, CLASS1_BLOCK_SAMPLES);\n\t\treturn 0;\n\t}\n\tcHDLCtx_preamble_state_init(ctx);\n\treturn 0;';assert old in f;x=x[:a]+f.replace(old,new)+x[z:]
  if idle:
   a,z,f=d.function(x,'_idle_state')
   old='\tif (n <= 0)\n\t\tn = CLASS1_BLOCK_SAMPLES;\n\tfor (i = 0; i < n; i++)\n\t\ttx[i] = 0;\n\t*tx_count = n;\n\treturn 0;'
   new='\tif (n > 0) {\n\t\tfor (i = 0; i < n; i++)\n\t\t\ttx[i] = 0;\n\t\t*tx_count = n;\n\t\treturn 0;\n\t}\n\tfor (i = 0; i < CLASS1_BLOCK_SAMPLES; i++)\n\t\ttx[i] = 0;\n\t*tx_count = CLASS1_BLOCK_SAMPLES;\n\treturn 0;';assert old in f;x=x[:a]+f.replace(old,new)+x[z:]
  out[label]=x
 assert len(set(out.values()))==4
 return out
d.variants=variants
if __name__=='__main__':d.main()
