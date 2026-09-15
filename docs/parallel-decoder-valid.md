# Valid parallel decoder source-form experiment

This supersedes the separately preserved invalid `parallel-decoder-experiment` run. This run is rooted at `/tmp/slmodem-byte-fidelity`, uses the tree's `byteident` and `partialcmp` authorities, and compares its unchanged output byte-for-byte with its retained `build/tc_repro` object.

## Validity and toolchain

The unchanged control is raw-object identical. The published Gentoo GCC 3.4.2 image, explicit compiler path, native image user, exact retained `.build-config` flags, overlay include first, and `DSPLIB_REPRODUCE_BUGS` last were used. Both `gcc` and `g++`, plus the assembler actually selected by `g++`, are recorded in `toolchain.txt`.

The denominator is 12 symbols shared by the full translation unit and reference. No option varied. Every overlay retained the current encoder body unchanged, and both candidates preserve `unsigned i;` before `T *state = state_;`.

## Results

| form | decoder bytes | canonical verdict | exact set | changed bodies | allocated non-text bytes | symbol records |
|---|---:|---|---:|---|---|---|
| `unchanged` | 62 | SIZE/5 | 8/12 | `none` | equal | equal |
| `hoisted-state-indexed` | 90 | SIZE/33 | 8/12 | `_ZN27ParallelDifferentialDecoderIhE7processEPhS1_` | equal | DIFF |
| `all-cursors` | 58 | SIZE/1 | 8/12 | `_ZN27ParallelDifferentialDecoderIhE7processEPhS1_` | equal | DIFF |

The reference decoder is 57 bytes. The all-cursors form is 58 bytes and its canonical verdict is `SIZE/1`; the hoisted indexed form is larger. The exact changed-body list for all-cursors is: `_ZN27ParallelDifferentialDecoderIhE7processEPhS1_`. No other shared function body changes.

All allocated non-text section contents are compared as actual bytes, not sizes.
Full ELF symbol records include name, type, binding, visibility, owning section,
value, and size. The candidate records differ only in the decoder helper's
expected size change; names, bindings, visibility, sections and values remain
unchanged. The table above, rather than the initial report's equality claim,
records the actual comparison.

## Actual 57/58 disassembly difference

```diff
--- reference-57
+++ all-cursors-58
@@ -1,120 +1,8 @@
 
-/tmp/slmodem-byte-fidelity/ref/slmodemd/dsplibs.o:     file format elf32-i386
+/tmp/slmodem-byte-fidelity/build/parallel-decoder-valid/all-cursors.o:     file format elf32-i386
 
 
 Disassembly of section .text:
-
-Disassembly of section .gnu.linkonce.t._ZN10GenericIIRIfdE5resetEv:
-
-Disassembly of section .gnu.linkonce.t._ZN10GenericIIRIfdE7processEPKfPfj:
-
-Disassembly of section .gnu.linkonce.t._ZN8SineWaveIffED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN10GenericIIRIfdED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN8SineWaveIffEC1Effff:
-
-Disassembly of section .gnu.linkonce.t._ZN8SineWaveIffE8generateEPfm:
-
-Disassembly of section .gnu.linkonce.t._ZN10GenericIIRIfdEC1EjjPdS1_j:
-
-Disassembly of section .gnu.linkonce.t._ZN10GenericIIRIfdE7processEf:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfE5resetEv:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfEC1Ej:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfE4readEPfj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihE19resetHistoryIndexesEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihE5resetEi:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihEC1Ejjj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihE15copyHistoryTailEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIihE7processEPKiPhj:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfE5writeEPfj:
-
-Disassembly of section .gnu.linkonce.t._ZN5QueueIfE5writeEf:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiE19resetHistoryIndexesEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiE5resetEh:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiEC1Ejjj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiE15copyHistoryTailEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhiE7processEh:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE19resetHistoryIndexesEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE5resetEh:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhEC1Ejjj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE15copyHistoryTailEv:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE14processAllOnesEPhj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE15processAllZerosEPhj:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE7processEh:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN3AgcIfE6freezeEv:
-
-Disassembly of section .gnu.linkonce.t._ZN3AgcIfE5resetEv:
-
-Disassembly of section .gnu.linkonce.t._ZN3AgcIfEC1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN3AgcIfE7processEPKfPfj:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiE19resetHistoryIndexesEv:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiE5resetEh:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiEC1Ejjj:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiE15copyHistoryTailEv:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiE7processEPKhPij:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN25SerialDifferentialDecoderIiE7processEi:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiE19resetHistoryIndexesEv:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiE5resetEi:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiEC1Ejjj:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiE15copyHistoryTailEv:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIiiE7processEi:
-
-Disassembly of section .gnu.linkonce.t._ZN11DescramblerIhiE7processEh:
-
-Disassembly of section .gnu.linkonce.t._ZN9ScramblerIhhE7processEPKhPhj:
-
-Disassembly of section .gnu.linkonce.t._ZN25SerialDifferentialEncoderIhE7processEh:
-
-Disassembly of section .gnu.linkonce.t._ZN25SerialDifferentialDecoderIhE7processEh:
-
-Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialDecoderIhEC1Ej:
 
 Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialDecoderIhED1Ev:
 
@@ -134,61 +22,25 @@
   12:	8b 45 08             	mov    0x8(%ebp),%eax
   15:	8b 4d 00             	mov    0x0(%ebp),%ecx
   18:	85 c0                	test   %eax,%eax
-  1a:	74 18                	je     34 <_ZN27ParallelDifferentialDecoderIhE7processEPhS1_+0x34>
+  1a:	74 19                	je     35 <_ZN27ParallelDifferentialDecoderIhE7processEPhS1_+0x35>
   1c:	8d 74 26 00          	lea    0x0(%esi,%eiz,1),%esi
   20:	0f b6 16             	movzbl (%esi),%edx
   23:	47                   	inc    %edi
   24:	46                   	inc    %esi
-  25:	88 d0                	mov    %dl,%al
-  27:	32 01                	xor    (%ecx),%al
-  29:	88 03                	mov    %al,(%ebx)
-  2b:	43                   	inc    %ebx
-  2c:	88 11                	mov    %dl,(%ecx)
-  2e:	41                   	inc    %ecx
-  2f:	39 7d 08             	cmp    %edi,0x8(%ebp)
-  32:	77 ec                	ja     20 <_ZN27ParallelDifferentialDecoderIhE7processEPhS1_+0x20>
-  34:	5b                   	pop    %ebx
-  35:	5e                   	pop    %esi
-  36:	5f                   	pop    %edi
-  37:	5d                   	pop    %ebp
-  38:	c3                   	ret
+  25:	0f b6 01             	movzbl (%ecx),%eax
+  28:	30 d0                	xor    %dl,%al
+  2a:	88 03                	mov    %al,(%ebx)
+  2c:	43                   	inc    %ebx
+  2d:	88 11                	mov    %dl,(%ecx)
+  2f:	41                   	inc    %ecx
+  30:	39 7d 08             	cmp    %edi,0x8(%ebp)
+  33:	77 eb                	ja     20 <_ZN27ParallelDifferentialDecoderIhE7processEPhS1_+0x20>
+  35:	5b                   	pop    %ebx
+  36:	5e                   	pop    %esi
+  37:	5f                   	pop    %edi
+  38:	5d                   	pop    %ebp
+  39:	c3                   	ret
 
-Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialEncoderIhEC1Ej:
+Disassembly of section .gnu.linkonce.t._ZN25SerialDifferentialDecoderIhE7processEh:
 
-Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialEncoderIhED1Ev:
-
-Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialEncoderIhE5resetEjh:
-
-Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialEncoderIhE7processEPhS1_:
-
-Disassembly of section .gnu.linkonce.t._Z3sumIfET_PS0_j:
-
-Disassembly of section .gnu.linkonce.t._Z4meanIfET_PS0_j:
-
-Disassembly of section .gnu.linkonce.t._Z6sqrSumIfET_PS0_j:
-
-Disassembly of section .gnu.linkonce.t._Z3VarIfET_PS0_j:
-
-Disassembly of section .gnu.linkonce.t._ZN9Resampler16timingCorrectionEf:
-
-Disassembly of section .gnu.linkonce.t._ZN10LowPassFIRIfED1Ev:
-
-Disassembly of section .gnu.linkonce.t._Z6boxcarIfEvPT_j:
-
-Disassembly of section .gnu.linkonce.t._Z7hanningIfEvPT_j:
-
-Disassembly of section .gnu.linkonce.t._Z7hammingIfEvPT_j:
-
-Disassembly of section .gnu.linkonce.t._Z8blackmanIfEvPT_j:
-
-Disassembly of section .gnu.linkonce.t._Z12designWindowIfEv10WindowTypePT_j:
-
-Disassembly of section .gnu.linkonce.t._Z4sincIfET_S0_:
-
-Disassembly of section .gnu.linkonce.t._ZN10LowPassFIRIfE6designEjffPKfi:
-
-Disassembly of section .gnu.linkonce.t._ZN10LowPassFIRIfE6designEjf10WindowTypef:
-
-Disassembly of section .gnu.linkonce.t._ZN10LowPassFIRIfEC1Ejf10WindowTypef:
-
-Disassembly of section .gnu.linkonce.t._Z3StdIfET_PS0_j:
+Disassembly of section .gnu.linkonce.t._ZN27ParallelDifferentialDecoderIhEC1Ej:
```

## Interpretation

The all-cursors spelling is valid evidence of a partial code-generation improvement: it reduces the decoder by four bytes and leaves it one byte from the reference while changing only the intended body. It does not establish source adoption by itself; behavioral consumer and period differential gates remain independent acceptance evidence. Exact-count gain is not imposed as a prerequisite.
