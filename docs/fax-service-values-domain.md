# Fax service scalar values before dispatch

Base a95a6c65; four full fax.c source cells, two binary axes, retained Gentoo
flags. Only FAX_class1_command and FAX_create, no source/header/layout fitting.

- Original FAX_class1_command initializes EBP to0 before dispatch; FTM branch
  assigns0x50; common fax_class1_command fourth argument comes from EBP. Source
  declares `int extra;` and assigns only on FTM, so GCC forwards0x50 on every
  other arm via undefined uninitialized use. Test ordinary `int extra = 0`,
  preserving FTM's assignment, all command/rate/debug/return behavior. This
  recovers an independently witnessed argument, not a value score fit.
- Original FAX_create writes local.mode into its stack config before the
  external modem_get_sreg call. Retained source computes/writes it afterward.
  Move existing side-effect-free mode assignment before that call, preserving
  zero template, S7 raw capture/stores/debug/create/cleanup and every value.
  Local config is not exposed to that call; this ordinary lifetime boundary
  changes no public behavior. No other constructor stores reordered.

Predictions: missing initialized dispatch value may restore EBP carrier/common
call and complete command; pre-call config lifetime may recover constructor.
Full-function nonexact falsifies any candidate as complete preimage; keep useful
negatives. All four cells evaluated, raw full baseline repeat, every emitted
body/binding/data/BSS/canonical nontext relocation reviewed; bugdefine last and
actual compiler/assembler commands retained. No runtime/fuzz/mutation; parent
owns source adoption and final combined phase gate if exact gains exist.

Follow-up, declared after the initial four cells: seven full fax.c controls,
baseline and two independent two-bit source products (three nonbaseline each).

Command cross initialization with complete six-case switch. Original command
dispatch has a six-entry relocated jump table; current explicit range guard
admits six but switch has five cases and FRH default, adding CMP4 before its
five-entry jump table. Replace that default with explicit FAXC1_FRH and a
separate default return-1 (unreachable under retained cmd<=5 guard). No command,
rate, debug or call changes. Prediction: recover six-way dispatch and original
initialized-extra carrier jointly; falsifier full body not exact.

Constructor cross mode-before-S7 with common converter result. Original clears
EAX before rate tests, returning calls meet one ctx->rc_a/rc_b store and test
EAX. Current branch stores duplicate rc_a writes and then reads the member.
Use one existing typed `struct rc *rc`, initializedNULL before rate cases,
assigned returned converter, stored into owner and checked as same result;
resetNULL before second conversion. No saved slots/registers/forced casts;
this is original branch-result/publication factoring. Exactly two independent
result choices, existing modes and calls unchanged. Full body decides, never
count or size alone. Repeat extra-only/mode-only cells to control context.
