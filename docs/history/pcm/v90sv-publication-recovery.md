# V90SpectralVerifier owned-Psd publication

At014667c0 both constructors are158B versus the blob162B. Reference C1
calls Psd constructor at0x45bcd then publishes its address at owner+4
at0x45bd2; C2 repeats0x45b1d/0x45b22. Baseline stores owner before call.
This is an observable source lifetime boundary, not register-colour fitting.

[Predeclared three-cell complete-TU domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947205616): unchanged early publication, raw local plus placement construction then publication, direct assignment of placement-new expression. Both late forms emit the same complete object and make both162B clones EXACT. Three sources/two emissions; baseline raw-reproduces retained production. Fourteen functions/global bindings/data preserved; only C1/C2 canonical bodies/relocations change. Nine/fourteen ->eleven/fourteen exact, no losses.

Retain direct placement-expression assignment, preserving existing allocation
operator, two float allocations and member-based parameter reloads. There
is no new allocator wrapper, definition-placement or register permutation.
Psd constructor and sysdep_malloc relocations remain; no null check appears.
Failure behavior is unchanged at this boundary: allocation null is passed
to construction just as in reference. No forced allocation-failure fixture
claim. Existing t_v90spectral alternates both constructor/destructor clones,
compares owner and nested Psd/window state, untouched fields/buffers, five
allocations per side with exact byte accounting, reset and destruction.
Two static anchors retargeted with their same fault meanings (clear spectrum
and wrong nested length). No fuzzing or mutation execution.

F1240 maps fields; F10164 restores placement construction; neither closes
this publication boundary. F7985 concerns converter spellings. F11579.
Replay tools/playbook_v90sv_publication.py --domain URL above. Actual complete
configured CXX flags, mandatory DSPLIB_REPRODUCE_BUGS, Gentoo3.4.2-r2 and
selected executed assembler2.15.92.0.2 identities, sources/object hashes,
inventories/changed disassembly/RTL saved in build/playbook-v90sv-publication.
Adoption artifacts: build/playbook-v90sv-publication-adoption.

Retained300/300 build changes only src_pump_v90_V90SpectralVerifier.cpp.o;
whole object raw-reproduces winner. Whole-tree874/1852 ->876/1852 exact
and85,291 ->85,615 exact bytes; only constructor clones gain, no losses.
Complete300-object partial links, same recovered order, remain DIFFERENT
(strict exit1). Positioned68,418 ->68,568/943,398 bytes; allocated914,094
->914,126; exact section70/92 and symbol394/2907 unchanged, relocation
1,018 ->1,025/18,317. Full-object/profile identity remains open.

Fixed Gentoo phase385 passed/0 failed, including constructor/nested allocation
accounting/reset/destructor fixtures. Structural14,235 references/2,708
findings headings clean;285 suites/10,038 static anchors clean.
