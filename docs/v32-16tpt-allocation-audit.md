# V.32 16-point trellis allocation audit

The root-pointer reloads are explained by a concrete allocation conflict. This
2026-11-04 audit finds no new source discriminator and makes no reconstruction,
compiler-profile, test or grader change. It starts from `52854aaa0` (PR #582).

## Observed allocation

The current faithful `V32dec.c` object is raw identical to the saved
`helper-owner-result/candidate.o`, SHA-256
`a836f91b4eaf6e8211f8cadbbd63f912e7f038fdfefb7f1cc9646f246961a35b`.
The corresponding Gentoo GCC 3.4.2-r2 dumps therefore describe the current
candidate. Its saved full-TU experimental control was verified in finding F12129.
This audit runs zero new experimental compiles.

The `.25.greg` dump orders the fourteen unallocated pseudos as:

```
135 68 167 67 66 74 87 111 70 88 64 63 69 59
```

`state`, pseudo 59, comes last. Its initial hard conflicts exclude AX, DX,
CX, BX and SP. Its preferred class is GENERAL_REGS with no alternate class.
The remaining integer choices are SI, DI and BP. All three are taken by earlier
global allocations that conflict with state:

| Value | Pseudo | References / live length in `.24.lreg` | Observed home |
|---|---:|---:|---|
| Selected owner | 74 | 20 / 95 | SI |
| Quality energy | 87 | 7 / 24 | DI |
| Received I sample | 63 | 3 / 46 | BP |
| State pointer | 59 | 9 / 116 | Unassigned; incoming argument home reused |

These reference counts alone do not determine priority. Upstream GCC 3.4.2
`global.c:623–646` orders allocations using floor(log2(reference count)),
weighted reference frequency, live length and size. The actual patched compiler's
order is directly observed above; exact numeric priority inputs are not printed
in this dump. The entry block has frequency 1100; the search-loop blocks have
frequency 10000. Those are compiler estimates, not measured modem frequencies.

`find_reg` combines class/fixed exclusions with propagated hard conflicts.
After assigning a global pseudo, upstream `global.c:1287–1318` propagates its
hard register to conflicting allocations and clears the local-only usage marker.
Its eviction route at `global.c:1230–1283` considers eligible local allocations,
not an earlier global allocation whose marker has been cleared. All saved local
homes are AX/DX/CX/BX. Checking the fallback requires more than this fact:
its exclusion mask differs from the ordinary search mask. Earlier global
assignments also occupy AX (135/67/88), DX (167/111), CX (68/66) and BX (70).
Together with SI/DI/BP, every allocatable integer register has had its local-only
marker cleared before state is considered. Consequently neither the ordinary
search nor the local-eviction fallback can give state a home.

The diagnostic reconstruction of that blocker set accepts the actual conflict
set. Removing each of its three blocker edges independently makes the set cease
to exhaust the choices: one positive and three negative controls. This checks
the saved-edge analysis; it does not execute or instrument GCC.

State's incoming load has a `REG_EQUIV` memory note. Reload can reuse that home
rather than allocate a new spill slot (`reload1.c:710–768,924–936`). The dump
separately logs pseudo 64, the Q sample, becoming a stack value. Upstream
`reload1.c:3675–3684` logs changed register homes; state has no corresponding
transition. Its final memory reloads and exhausted global choices support an
unassigned root entering reload, rather than a root first evicted with Q.
Upstream source explains these mechanisms; the patched compiler's dumps establish
the observed order, conflicts and output. This audit has not established that the
upstream files are identical to Gentoo's patched files. No original native RTL
is available.

## Native lifetime constraints

The reference loads the incoming state once into BP at `0x80337`. It retains
that value through quality, the default-arm owner reread at `0x804c3`, and the
post-quality sample accesses at `0x803da`, `0x803e0` and `0x803e3`. BP is then
reused at `0x803e6`. The current candidate contains four static loads of the
incoming state home, including two near entry; the previous shorthand about
two root loads did not describe the complete function.

The reference saves the unadjusted selected owner at stack +0x14: SI at
`0x803d6` on the fast path, fresh AX at `0x804cb` on the default path. That
owner has no register home through the nearest-point loop. It is reloaded into
DI at `0x8049b`, adjusted by 0x18 at `0x804a7`, and passed to `VTB_decoder` at
`0x804ad`. Both received samples stay in registers through the search.

This is a materially different lifetime/allocation arrangement from the
candidate, which retains owner 74 in SI across the loop and spills Q. It does
not reveal the original pseudo identities, priority order or source declarations.

## Admission decision and next evidence

A consumed local `struct vtb *v = &m->vtb` before the loop was considered and
not compiled. It is natural C, but the native code saves the unadjusted owner
and forms the member address after the loop. The hypothesis would rely on an
unobserved early address lifetime being removed or rematerialised by GCC to
recover that output. It can test allocator susceptibility, but the present
observation does not select that source boundary. This does not prove that
such an alias was absent from the original source.

Reopen a source experiment when evidence identifies a change to one of these
actual allocation inputs:

- A supported dependency or conversion changes one of the overlapping live
  ranges, rather than merely renaming or redeclaring it.
- A native-supported capture or use changes the reference graph and hence the
  allocation order. Do not add consumers to increase priority.
- Caller/callee or member evidence changes the required register class or
  identifies a distinct component value that the current graph conflates.
- An independently supported build setting predicts the whole TU, including
  its already recovered branches, ABS forms and call boundaries.

Logging numeric priority inputs in an instrumented exact compiler would make
the present decision easier to inspect, but would not itself identify the
original inputs. The observed order and complete blocker set already explain
this rejection. A candidate needs an additional native constraint before that
instrumentation is worth a separate build.

The audit stops without a source variant or function promotion. The unchanged
baseline remains 1205 EXACT, 39 REGALLOC, 608 NONMATCH and zero unresolved out
of 1852, as measured for F12129; this is not a new census. Saved dumps, the
allocation-evidence JSON and independent review remain under
`scratch/v32-allocator-audit/` and `scratch/v32-slicer-wave/` in the owned worktree.
