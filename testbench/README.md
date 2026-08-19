# Hardware interop bench

Calls between `slmodemd` (over SIP, via `d-modem`) and a real **SupraExpress
56e PRO**, reachable from the Asterisk PBX as extension **1901**. A **USR
Courier HST Dual Standard** answers on **1902**.

Modems are addressed by ROLE (`supra`, `courier`), never by `ttyUSBn` — see
"Naming the modems" below.

Purpose: find out what the reconstruction and the blob actually do against
hardware, as opposed to against each other. A blob-versus-blob call proves
sequencing; only a third-party modem tests whether the signals are right.

**Every tool in this directory is catalogued in [TOOLS.md](TOOLS.md)** —
what each one is for, which ones place real calls, which need a build from
`v34-instrumentation`, and a worked example of an impedance comparison. That
file carries a coverage check so a tool added without a line in it shows up as
UNDOCUMENTED.

## Safety

The PBX is live and can reach the PSTN, so the dial destination is restricted
in two places:

* `dmodem-guard-fork.sh` — `slmodemd`'s `-e` target, checks anything arriving
  in `argv`, exact match after stripping dial modifiers.
* `dmodem_dest_allowed()` in `d-modem.c` — checks the number arriving over the
  socket, which in this configuration is **the only one that exists**: the fork
  launches `d-modem` once at startup with an empty dial string and sends the
  number afterwards. A launcher-only guard is therefore not a guard at all,
  which is how the first three attempts failed.

Default allow-list is `1901`; `DMODEM_ALLOWED_DEST` overrides. Refusals are
exact-length, so `9W1901` (which cleans to `91901`) is refused rather than
prefix-matched.

Credentials come from `asterisk-login-4242.secret` and are passed as
`SIP_SERVER` / `SIP_USER` / `SIP_PASSWORD` environment variables, never as
`--sip-password`, which would put the secret in `argv` where `ps` shows it to
every user on the machine for the length of the call.

## Naming the modems, and refusing to dial without one

    testbench/modems.sh list          what is attached, and does it answer AT
    testbench/modems.sh pin           write modems.conf from what is attached
    testbench/modems.sh check supra   resolve + pre-flight one role

**`ttyUSBn` is not a stable name.** It is handed out in enumeration order. On
10 August the whole hub dropped; when it came back on the 11th the same two
modems were on `ttyUSB0` and `ttyUSB1`, having been `ttyUSB1` and `ttyUSB2`
before. Every script here hard-coded `/dev/ttyUSB1`, so the next run would have
driven the **Courier** while labelling every log line SupraExpress. Nothing
would have looked wrong.

So a role resolves through `/dev/serial/by-id/`, which carries the adapter's
serial number, in this order:

1. `$SUPRA_TTY` / `$COURIER_TTY` — explicit override
2. `testbench/modems.conf` — pinned by serial number, written by `modems.sh pin`
3. a glob over `/dev/serial/by-id/` — **ambiguity is an error, not a guess**

**A device node is not a modem, either.** Pre-flight sends `AT` and requires
`OK`, then reads the product string and checks it against the role's expected
identity, so "something answered" becomes "the modem I meant answered". It runs
**before** `slmodemd` starts and before anything dials; failure is exit **3**,
distinct from the allow-list's exit 2.

`atprobe.py` is the prober and works standalone:

    atprobe.py /dev/serial/by-id/usb-FTDI_... --expect SupraExpress

Its `ATI` order is not arbitrary. `ATI3` is the product ID on the Rockwell but
the **duration of the last call** on the Courier — `00:00:19`, a perfectly good
reply that identifies nothing, and which would have been pinned as that modem's
name had the first non-empty answer been taken. A candidate must contain a
letter, which drops the timer and falls through to `ATI4`.

## The ATA's terminating impedance: 600r and complex2 are two channels

The VG204's analogue ports used to carry no `impedance` line, so they ran at
the platform default of **600 ohm resistive**; they now carry **`impedance
complex2`** on all four voice ports. Nothing else in the gateway config moved.
Terminating impedance sets the 2-to-4-wire hybrid match, so it moves
trans-hybrid loss, the reflected frequency response and the level transfer.

**2,037 of the 2,039 archived capture logs predate the change**, so every
number in this file that is not marked complex2 is a 600r number.

    bandshape.py --label 600r a/*.slmodemd.log --label complex2 b/*.slmodemd.log

`bandshape.py` reads the **V.34 line probe** out of `-d9` logs (25 bins,
150 Hz apart, 150–3750 Hz — needs `DSPLIB_V34_DUMP_PROBE_BINS=1` and a hybrid
build). It is the right instrument for this and the chirp is not: the chirp
injector was an uncommitted change to `d-modem.c` that commit df93682d
reverted, and its sweep stopped at ~3000 Hz, which is below every point in
dispute. The probe is emitted before the far end applies pre-emphasis, which
finding 1907 says is the only safe window for a channel measurement.

Measured (dB relative to the 750 Hz reference bin), 600r = 112 calls /
354 probes, complex2 = 10 calls / 23 probes:

| Hz | 600r | complex2 | delta |
|---|---|---|---|
| 1050 | +0.01 | −0.28 | −0.29 |
| 2100 | −0.11 | −1.71 | −1.60 |
| 3150 | −0.75 | −3.51 | −2.76 |
| 3300 | −1.16 | −4.08 | −2.92 |
| 3450 | −2.14 | −5.16 | −3.02 |
| 3600 | −5.94 | −9.12 | −3.18 |
| 3750 | −13.71 | −17.06 | −3.35 |

**THIS BENCH NOW HAS TILT, AND IT DID NOT BEFORE.** In-band slope over
450–3150 Hz (finding 1956's band) goes **−0.60 dB → −3.72 dB**, monotonic bin
by bin, interquartile range under 0.05 dB. Task #162 recorded that this bench
"physically cannot show a pre-emphasis benefit" because the path measured
flat. That is no longer true and **#163 is runnable for the first time**.

The negotiated symbol rate went with it: **3429 baud on the 600r arm, 3200 on
10 of 10 complex2 calls**, which is what the band edge losing 3 dB should do.

The shared C model carries both as selectable profiles — `CHAN_LINE_MODEL=vg204-1907`
(the default, unchanged), `vg204-600r-probe`, `vg204-complex2-probe` — with
the provenance of each beside its coefficients. Every run prints which one it
used. **The default is deliberately still 1907's fit even though the probe
says that fit is about 6 dB too steep through the roll-off**, because every
archived emulator result was taken with it.

## Measuring the echo

    echoscan.py captures/asym-1           # full report, lags and noise floor
    echoscan.py --selftest                # plant a known echo, watch it appear
    echofit.py  captures/asym-1           # three numbers, for a table column
    echo-compare.sh courier captures/ec-courier 5

**`echoscan.py` could not run at all between its last edit and task #168** —
`from capture_io import load` then `def load(path): a = load(path)[0]` shadowed
the import with its own wrapper and recursed until the stack went (finding
1971). It is repaired, it reports its denominators, and `--selftest` plants
finding 1971's ladder and checks it comes back: null −39.3 dB, and −10/−20/−30
dB echoes recovered at −10.31/−19.72/−29.05 dB, all at the planted 30.00 ms
lag.

**And repairing it overturned the finding that retired the question.** There
*is* a linear echo on this path, at **171.5 ms in 19 of 20 calls to within
0.25 ms** — the same quantity 1204/1215/1216 measured at 205.62 and
165.62/175.62 ms. `echoratio.py`, which produced 1971's "below −25 dB, no
linear echo" bound, cannot see it: its lag search caps at 120 ms and its
coherence window is 128 ms, both shorter than the delay. A −20 dB echo planted
at 171.5 ms reads **−24.56 dB (its null floor) at a lag of 47.8 ms** to
`echoratio` and **−20.15 dB at 171.50 ms** to `echoscan`. A mismatched-pair
control — one call's transmit against another call's receive — scatters at
98/280/239 ms and −43 to −45 dB, so the 171.5 ms peak is not an artefact of
the file structure.

| | 600r (n=10) | complex2 (n=10) |
|---|---|---|
| lag | 171.38–171.62 ms | 171.50 ms (one at 161.5) |
| echo/received, median | **−29.8 dB** | **−24.0 dB** |

So the better-matched hybrid returns **~6 dB more** of our own signal, and it
sits about 1 ms inside the canceller's 172.5 ms delay line (1216).

`row.sh` writes both directions from the same loop on the same timebase, so
they line up sample for sample and a cross-correlation of transmit against
receive needs no alignment step. That makes "is our own signal coming back, and
when" a measurement rather than an inference.

It found the thing four other hypotheses were circling: **our transmit returns
at 205.62 ms, about 19 dB down, the same lag in five calls out of five to
within one sample.** The V.34 canceller reaches `IODELAY + 60` samples at
9600 Hz — 31.25 ms at the maximum IODELAY of 240 — so the reflection is 6.6x
beyond anything the knob can address, and the receive rate sits at exactly the
SNR that 19 dB of uncancelled echo implies.

The one thing this cannot see is a **non-linear** echo: G.711 companding
returns energy that is not a scaled copy of what we sent, and no correlation
will find it. A null result bounds the linear echo only.

## Running a row

    testbench/row.sh <logprefix> pty|tty [number]

`pty` means `slmodemd` originates, `tty` means the SupraExpress does. `TTY=`
takes a role name (`supra`, `courier`) or a raw `/dev` path. Artefacts land in
`testbench/captures/`.

## What each call records

| file | rate | content |
|---|---|---|
| `*.modem_rx_8k.wav` | 8000 | SIP audio inbound — the far modem, as carried |
| `*.modem_tx_8k.wav` | 8000 | SIP audio outbound, **after** the resampler |
| `*.modem_rx.wav` | 9600 | the same inbound audio as the datapump sees it |
| `*.modem_tx.wav` | 9600 | what the datapump generated, before resampling |
| `*.call.log` | | both DTEs, timestamped, interleaved |
| `*.slmodemd.log` | | `-d9` trace including the V.8 shim status line |

Having both rates is what separates a modem fault from a resampler fault. The
speaker on the real modem is turned up for the whole call (`ATM2L3`) so the
handshake is audible.

`v21decode.py` reads the V.8 messages straight out of a recording:

    v21decode.py <file.raw> 8000 ch1   # originating modem, 980/1180 Hz
    v21decode.py <file.raw> 8000 ch2   # answering modem, 1650/1850 Hz

This is the only way to see what the sealed hardware modem actually said.

## The tools

A lab notebook is not an index, so here is one. Every script carries its
reasoning in its own header; this is only the map.

**Placing calls.** `row.sh` runs one call (the workhorse), `hw2hw.sh` puts two
hardware modems together with no software in the path, `relaycall.sh` bridges
two SIP legs. `dial.sh` is the low-level dialler. `waitquiet.sh` blocks until
the machine is quiet enough to measure on — a call taken on a loaded box
degrades silently, which is worse than failing.

**Batches.** `ab149.sh` is the pre-registered A/B harness (gates on a quiet
machine before *every* call, not just the first); `ladder.sh` walks one
modulation per call; `preemph_ab.sh` / `preemph_ab2.sh` / `preemph_fit_ab.sh`
are the pre-emphasis arms; the `*-sweep.sh` scripts vary one parameter.

**Impedance sweeps.** `linesweep.py` measures, stores, models, compares and
plots the line response under different ATA impedance settings, per modem —
`ingest` builds an arm from existing capture logs, `measure` places the calls
itself, and `list` / `compare` / `plot` read the store in `linemodels/`. It
imports `bandshape.py` rather than re-implementing the reduction, so arms taken
months apart go through identical code.

Two things about it are deliberate and worth knowing before you trust an arm.
The impedance is **your assertion** — nothing here can interrogate the VG204 —
so pass `--ata-config /srv/tftp/incoming/vg204-runconf-new` and the tool stores
that file's sha256 and the actual `impedance` lines it found; `list` then shows
the arm as EVIDENCED rather than asserted. And a bin floored on more than half
the probes is stored as **null**, not as its floor value, so `compare` prints
`--` and `plot` breaks the line: a floor is the instrument running out, not a
measurement of the channel.

The point of it is that the right impedance is probably not the same for every
modem. The Courier answers `Product type: UK External MSK` to ATI7, so its
front end was designed against BT line impedance (Cisco's `complex1`); the
Oli'Net and the SupraExpress are different designs of different vintage. Keep
every arm and compare, rather than picking one setting for the bench.

**No hardware needed.** `vbt-chanshim` puts two live datapumps on an emulated
channel (`chancall.sh` drives it) — band limit, delay, noise, loss and
`CHAN_SLIP`, the jitter-buffer underrun model. `hsfcall.sh` drives the same
channel with a DIFFERENT far end — the Conexant HSF datapump (`hsfshim.py`
makes the socketpair and rings it), which is the only readable, known-good
V.34 we can put opposite ours in the emulator; `chancall.sh` can only tell us
what we do against ourselves. `replay.py` feeds a recording
to slmodemd in d-modem's place, `replaydte.py` acts as the DTE, `replaycmp.sh`
compares two builds on one recording. `linesim.py` is the offline line model.

**Reading the results.** `ab149scan.py` scores an A/B exactly as its
pre-registration says (connect success first, then the fixed window).
`batchanalyse.py`, `abcompare.py`, `abextract.py`, `callstats.py` handle the
older batch formats. `freezescan.py` tests equaliser-freeze clustering against
a null. `echoscan.py` / `echofit.py` / `chirpdelay.py` measure the path.
`modemid.py` asks a modem what it is before trusting an init string.
`v21decode.py` and `v8analyse.c` decode V.8 out of a recording.
`probeplot.py` / `fitplot.py` plot probe bins and pre-emphasis fits.

**Keeping things.** `archive.py` copies the captures a finding or a tool
actually names to the ZFS store, in `<family>/<prefix>/` subdirectories, and
indexes them. See `records/README.md` for why `captures/` is ignored but
`records/` is not.

## Four harness defects worth not repeating

**The serial port must stay open for the whole call.** The first harness
opened the serial port, sent one AT command and closed it again, for each
interaction. While the port is closed the kernel discards everything the modem
sends, so `RING`, `CONNECT <rate>` and `NO CARRIER` — the entire answering-side
record — were thrown away, and the log read "(nothing buffered)", which looks
like the modem said nothing. Closing the port also drops DTR, so observing
changed the thing observed. Three calls were analysed without this.

**The audio dumps are reopened every run.** They are now copied into
`captures/` at teardown; before that, each call destroyed the evidence for the
last one, and the IODELAY 48-versus-240 comparison was lost.

**The test did not notice that its modem was missing.** The asymmetry run made
five calls into an empty line — the hub had dropped every adapter at once — and
tabulated five rows of `no connect`. A row that says `no connect` because the
modem is unplugged is indistinguishable, in the summary, from one that says
`no connect` because V.34 failed, and this bench has already been burned once
by reading noise as signal. The only check was an `open()` inside each call,
i.e. *after* the dial had gone out, and it did not stop the run. Hence the
pre-flight above, which is exercised in both directions rather than assumed to
work: a missing node, a node with nothing behind it (a silent pty), a modem
whose identity does not match the role, and the live modem.

### `pgrep -f <pattern>` always matches itself

Checking whether something is running with `pgrep -f ab149.sh` reports a match
whether or not it is running, because the shell invoking pgrep has the pattern
in its own command line and `/proc/*/cmdline` is exactly what `-f` searches.
pgrep excludes its own pid, not its parent's. Demonstrated in one line:

    $ pgrep -f "zzz-no-such-process-zzz" | wc -l
    1

This has twice sent someone hunting a process that had already exited. Use:

  * `pgrep -x slmodemd` -- matches the executable NAME, so a shell holding the
    string cannot match. `relaycall.sh` already does this.
  * a pidfile written by the process itself. `row.sh` does this so teardown is
    by PID, which also avoids the older trap in its header comment: `pkill -f
    d-modem` matches this repository's PATH and once killed an unrelated agent.
  * `pgrep -f "[a]b149"` where only `-f` will do -- the regex matches `ab149`
    while the literal text in the command line is `[a]b149`, which does not
    match itself.

`pgrep -f X | grep -v $$` does not reliably help: the self-match is usually the
parent wrapper, not `$$`.

## Results

### Row 2 — blob originates over SIP, SupraExpress answers: FAILS in V.8

Reproduced five times, identical each time.

The call itself is fine. SIP registers, `1901` rings the SupraExpress (`RING`
at 7.3 s in the call log), it auto-answers, RTP flows both ways, and the audio
is clean: **ANSam at 2100 Hz arrives at 8.0 s** and the blob answers it with a
well-formed V.21 channel-1 CM — the eight octets `e0 c1 65 11 10 0d 27 10`,
repeated every 300 ms from 9.78 s. The far end replies with V.21 channel-2
traffic and a short structured burst at 16.3–17.15 s.

Then both ends stop. Neither trains. Both sit on steady tones until the
SupraExpress times out at 60 s and both report `NO CARRIER`.

**Where it dies is exact.** The V.8 shim status sequence is

    0  V8_INIT
    6  V8_ORG_WAITING_FOR_ANSAM
    7  V8_ORG_ANSAM_DETECTED_WAITING_TE
    14 V8_ORG_SEND_QC
    8  V8_ORG_SEND_CM      <-- and it never leaves

with `dp_requested=0` throughout. Status 9 is `V8_ORG_JM_DETECTED` and is never
reached: **the blob never recognises the SupraExpress's JM**, so it never
selects a datapump. (The status names are the original author's, recovered from
`v8StatusName` at `.rodata+0x53c0` — see `claude_re/include/dsplib/v8.h`.)

Two candidate causes were tested and eliminated:

* **`MDMPRM_IODELAY` is not the blocker.** Raising it from 48 (filtdelay 47,
  below the 57 the V.34 handshake needs) to 240 (filtdelay 95, the top of the
  useful range) changed nothing — same status sequence, same timings to the
  millisecond. `mov $0xf0,%edx` was confirmed in the linked binary, so the
  value was live. The change is kept because the derivation still says 48 is
  wrong for a SIP leg; it simply is not what fails here.
* **Audio starvation is not the blocker.** All 26 `scombdb-up Underflow`
  messages carry one timestamp, the instant media starts — the jitter buffer
  priming, not ongoing starvation.

**And the blob is not at fault: no JM is ever sent.** A 100 ms spectrogram of
the receive channel gives the far end's whole sequence:

| window | components | |
|---|---|---|
| to 13.2 s | 2100 Hz, pure | ANSam |
| 13.3-16.2 | 1800 + 2250 + 2850 | structured, not identified |
| 16.3-17.0 | 600 + 3000 | an 1800 Hz carrier with +/-1200 alternations |
| 17.1-19.0 | 1650 Hz, pure | V.21 channel 2, **mark, continuous** |
| 19.1 on | 1300 Hz, pure | parked until timeout |

The fourth row settles it. At 300 bit/s a 100 ms window spans about thirty
bits, so a JM carrying any data would show 1850 Hz as well as 1650 Hz. There is
no 1850 Hz component at all -- the traces at 0.05 relative amplitude are 1170
and 980 Hz, which is our own transmit echoing back through the PBX. The
SupraExpress raises its channel-2 carrier and puts **no message on it**.

`v21decode.py`'s octets for the 13.3-16.2 window are the artefact they were
suspected of being -- that window is not V.21 at all, and the decoder was
framing a signal it had no business framing.

**A SECOND MODEM OVERTURNS THE CONCLUSION THIS SECTION ORIGINALLY DREW.** It
said: "the blob is not at fault, this is a far-end behaviour". That was written
with one peer. See row 10 -- with two independent peers it no longer holds, and
the suspicion moves to our side.

### Row 1 -- SupraExpress originates, blob answers: FAILS the same way

The mirror image, and it fails symmetrically. `1901` dials `4242`, d-modem
takes the inbound INVITE, slmodemd rings (`RING` on the pty at 11.8 s) and
auto-answers. The blob then runs

    0 V8_INIT -> 1 V8_ANS_SEND_ANSAM -> 4 V8_ANS_TIME_OUT_WAITING_FOR_CM

Our ANSam goes out cleanly (2100 Hz, rms 2280, for the full 12 s). The
SupraExpress transmits in the V.21 **channel-1** band for a quarter of a second
at 3.5 s -- the start of a CM -- and then stops. From 4.5 s the receive channel
is not merely quiet but **exactly zero**: digital silence, for ten seconds,
while we are still transmitting.

A correction, because the first reading of this was wrong: the 2100 Hz present
in the receive channel from 2.0 to 3.25 s was described as "our own ANSam
echoing back". It is not. Cross-correlating our transmit against our receive
over 4.5-11.0 s gives **0.00 at every lag from 0 to 150 ms**, and the receive
channel there is identically zero. A path echo would have persisted for the
whole twelve seconds we were transmitting; this did not. The likeliest reading
is that the PBX or the ATA runs its own echo canceller which converges after
about a second, but that is a guess and it has not been tested. What is
measured is only this: **there is no measurable echo on this path**, which is
worth knowing before blaming the blob's echo canceller for row 6.

### Row 5 -- V.22bis forced on both ends: **CONNECTS, DATA BOTH WAYS**

(The first of four such rows; see "the ladder" below for V.22, V.32 and
V.32bis. This one is written out in full because it was the first call that
worked at all.)

    TTY_EXTRA="AT+MS=2,0" PTY_EXTRA="AT+MS=122,0" row.sh captures/row5 pty 1901

    25.79  tty  CONNECT 115200
    25.88  pty  CONNECT 2400
    27.14  pty  FROM-TTY 0123456789 the quick brown fox
    27.23  tty  FROM-PTY 9876543210 the quick brown fox
    === DATA BOTH WAYS: PASS

**This is the first working hardware interop call.** The blob's V.22bis
datapump connects to a real SupraExpress 56e PRO over SIP and carries data in
both directions, verified at both ends.

It also isolates the failure precisely. Everything shared by rows 2 and 5 is
now proven good against hardware: the SIP signalling, the RTP path, G.711 in
both directions, `RcFixed_Resample` at 8000<->9600, `MDMPRM_IODELAY` at 240,
the process plumbing, and the blob's ability to run a datapump to a working
link. What rows 2 and 5 do NOT share is V.8 -- and `AT+MS=<mod>,0` turns
automode off, so row 5's handshake never invokes it.

### The ladder -- rows 6 to 9, one modulation per call

Each forced on BOTH ends with `AT+MS=<mod>,0`, automode off, so no V.8 is
involved anywhere in these handshakes. Both ends acknowledged every setting
with `OK`, so in each case the modulation really was selected.

| row | modulation | `+MS` tty / pty | result |
|---|---|---|---|
| 7 | V.22, 1200 bit/s, 600 baud | 1 / 22 | **CONNECT 1200, data both ways** |
| 5 | V.22bis, 2400 bit/s, 600 baud | 2 / 122 | **CONNECT 2400, data both ways** |
| 8 | V.32, 2400 baud, echo cancelled | 9 / 32 | **CONNECT 14400, data both ways** |
| 6, 9 | V.32bis | 10 / 132 | fails, twice |

**V.32 passing overturns the conclusion drawn from V.32bis alone.** After row 6
it looked as though the path could not carry a 2400-baud echo-cancelled QAM
signal at all, and the echo-canceller delay was the obvious suspect. Row 8
kills that: V.32 is 2400 baud on an 1800 Hz carrier with echo cancellation, it
runs over this exact SIP path, and it carries 14400 bit/s in both directions.
So the path is not the limit, and `MDMPRM_IODELAY`/`echo_delay` is not the
explanation. Two independent measurements now point the same way -- the other
being that cross-correlating transmit against receive finds **no echo at all**
on this path (0.00 at every lag out to 150 ms), so there is little for a
canceller to get wrong.

What is left is oddly specific: **`DP_V32BIS` (132) fails where `DP_V32` (32)
succeeds at a V.32bis rate.** slmodemd never even reports a datapump for the
132 call -- it goes `DP_ESTAB` and then straight to `DP_DISC` at the timeout,
with no `CONNECT` -- whereas the 32 call reports `CONNECT` at 22 s. Since 32
negotiated 14400, which is above V.32's own 9600 ceiling, the blob's "V.32"
driver is evidently already doing V.32bis rates, and the separate 132 entry may
simply be a registration that does not work. That is a guess about the blob and
has not been checked against the disassembly.

Row 6 was run while a leaked `slmodemd`+`d-modem` group from an earlier query
was still registered to the PBX as 4242 -- a genuine confound, and the reason
row 9 exists. Row 9 repeated it on a clean tree and failed identically, so the
result stands; but rows 5 and 6 were both run under that contention and row 9
is the one to quote. The leak is fixed (see below).

### The process-group leak, and why the teardown missed it

`row.sh` derived the process group from `$!` after `setsid ... &`. When
`setsid` forks and exits, `$!` is already dead by the time the pgid is read,
the lookup returns empty, and teardown then signals nothing -- leaving a whole
slmodemd + d-modem group alive and still registered as 4242, to contend with
every later run. It survived seventeen minutes and four calls before being
noticed, by a human watching the process list rather than by anything in the
harness.

The fix: the shell that `setsid` puts in charge writes its own pid (which is
the new pgid) to a file and then `exec`s, so the value is exact and cannot go
stale; and teardown now verifies the group is actually gone and warns loudly if
it is not, because silence previously meant "signalled something", not "nothing
is left".

### What was tried and did NOT explain it

Recorded because each of these looked convincing and cost a call:

* **`+A8E` was not the cause.** The SupraExpress reports `+A8E?` as `0,0` --
  V.8 origination and answer negotiation both reading disabled -- and its own
  `+A8E=?` does not list `0` as a permitted value, which looked like a complete
  explanation. Setting `AT+A8E=1,1` and redialling reproduced the failure
  *exactly*: same status sequence, same audio, same timings. The setting also
  survives `ATZ`, `AT&F`, `&F1` and `&F2`, so it was not being reset underneath
  the test. Whatever `0,0` means on this firmware, it is not the gate.
* **`MDMPRM_IODELAY` was not the cause** (48 vs 240, no change).
* **Audio starvation was not the cause** (underflows are a start-up burst).

### Row 10 -- V.34 against a SECOND, UNRELATED modem: fails the same way

The second modem is a **USRobotics Courier HST Dual Standard V.34** on
extension 1902 (role `courier`) -- USR/TI silicon and the USR command set, with
no `+MS` and no `+A8E`, so nothing about the SupraExpress's configuration
carries over. slmodemd forced to V.34 (`AT+MS=34,1`), the Courier left in
automode (`AT&N0`).

    0 V8_INIT -> 6 V8_ORG_WAITING_FOR_ANSAM
      -> 7 V8_ORG_ANSAM_DETECTED_WAITING_TE -> 8 V8_ORG_SEND_CM   (stuck, 60 s)

Identical to the SupraExpress, and the audio is identical too. Both modems,
after answering, produce **the same sequence**:

    2100 Hz (ANSam)  ->  ~2250 Hz with a 500 Hz-spaced comb  ->  600 + 3000 Hz
      ->  V.21 channel 2 MARK, held, with no 1850 Hz component at all

The Courier holds that mark for **fifty seconds** while we transmit CM at it.
A modem holding its answer-channel carrier up, sending no data, for fifty
seconds, is a modem waiting for something it has not received.

**This changes the attribution.** With one peer, "that modem does not complete
V.8" was a fair reading. With two peers from different manufacturers, different
silicon and different firmware families behaving identically down to the tone
sequence, the common factor is us: the blob's CM is not being accepted, or is
not arriving in a form either peer will act on. The earlier text in this file
saying "the blob is not at fault" was written before this call and is wrong to
that extent.

Note also what this rules out. The same SIP path, the same resampler, the same
`d-modem`, the same `slmodemd` carry **V.22, V.22bis and V.32 at 14400 with
data both ways**. So the path is fine and the datapumps are fine; it is
specifically the V.8 exchange that fails, against both peers.

### Row 11 -- CONTROL: one hardware modem dials the other, no blob at all

`hw2hw.sh`: the SupraExpress (1901) dials the Courier (1902) across the same
PBX, with `slmodemd` and the blob nowhere in the call.

    12.13  pty  RING
    24.05  pty  CONNECT 28800/ARQ
    24.10  tty  CONNECT 115200

**28800 with ARQ is V.34 with V.42.** So both modems negotiate V.8 and train
V.34 to each other, over this PBX, in twelve seconds.

That kills, for good, every version of "these modems do not do V.8". It was
stated in this file twice on the strength of the SupraExpress alone and it was
wrong both times.

One qualification, stated because it is not established: 1901 and 1902 are both
analogue ports on the PBX, so this call may be bridged inside it without ever
traversing RTP. It proves the two modems interoperate at V.34 and that the PBX
carries V.34 between its own ports; it does not by itself prove the RTP leg to
`d-modem` is equivalent. The data probe also failed (both ends read garbage),
which is a harness issue with the `&F` defaults and flow control, not a link
failure -- the modems were connected and in ARQ.

### What SpanDSP says -- a third implementation as referee

`v8analyse.c` runs SpanDSP's V.8 receiver over a recording. SpanDSP is neither
the blob nor our reconstruction, and our reconstruction is derived from the
blob, so it is the only impartial reader available.

    v8analyse <file>_8k.raw answerer   # read the ORIGINATOR's CM
    v8analyse <file>_8k.raw caller     # read the ANSWERER's ANSam and JM

**The blob's CM is valid.** From row 10's transmit recording:

    FLOW V.8 >CM:  c1 45 11 10 2a 0d
      call function  6 (V series modem data)
      modulations    0x00000a00  ->  V.32/V.32bis, V.34
      protocol       1

SpanDSP parses it without complaint and reads V.34 on offer. So the blob is
sending a well-formed CM that advertises the right thing.

**And the far end does reply.** From row 2's receive recording:

    FLOW V.8 'ANSam/' recognised
    FLOW V.8 <CM:  c1 65 13 94 2a 0e
    FLOW V.8 <V.92:  00
    FLOW V.8 <CM:  c1 65 13 94 2a 0e
    FLOW V.8 Timeout waiting for JM
      call function  6 (V series modem data)
      modulations    0x00001a16  ->  V.21, V.22, V.23, V.32/V.32bis, V.34, V.90

**CORRECTION.** This file previously said the SupraExpress "raises its
channel-2 carrier and puts no message on it", concluding no JM was ever sent.
That was wrong. It was based on 250 ms spectrogram windows showing 1650 Hz
without 1850 Hz -- too coarse to see a message transmitted only twice. SpanDSP
decodes the message, and its contents are sensible: V.34 and V.90 offered.

What survives is narrower and stranger: **SpanDSP, an independent
implementation, ALSO fails to accept that reply** -- "Timeout waiting for JM".
Two independent readers, the blob and SpanDSP, both decline it. And the far end
sends it only twice, where V.8 has the answerer repeat JM until it detects CJ.

### V.34 rate: the far end does 33600, we do 14400 — and a warning about method

Once the SupraExpress is made to report its LINE rate rather than its DTE rate,
the picture changes completely. `ATW2` is a red herring on this firmware: it
returns `OK`, `ATW?` still reads 000, and `&V` still shows `W0`, because **ATW
and S95 are the same register**. `ATS95=47` is what works — bit 0 switches
CONNECT to the DCE rate, and 47 adds `/ARQ`, CARRIER, PROTOCOL and COMPRESSION.

Sweeping IODELAY across the usable band and scoring on rate:

| IODELAY | filtdelay | echo_delay | far end | blob | data |
|---|---|---|---|---|---|
| 88 | 57 | 148 | no connect | — | — |
| 104 | 61 | 164 | **33600/ARQ** | 14400 | PASS |
| 120 | 65 | 180 | **33600/ARQ** | 14400 | PASS |
| 136 | 69 | 196 | no connect | — | — |
| 152 | 73 | 212 | **33600/ARQ** | 14400 | PASS |
| 168 | 77 | 228 | **33600/ARQ** | 12000 | PASS |

**The far end runs full-rate V.34 on every call that connects.** So the channel
supports 33.6k, G.711 is not the limiting factor, and the earlier report of
"12000-14400" was partly an artefact of never seeing the far end's real rate.
Our side sits at 14400 while the peer manages 33600, which makes this a V.34
rate-selection question about the blob rather than a transport question.

**AND A WARNING ABOUT EVERY TABLE IN THIS FILE.** 136 fails while 104, 120 and
152 pass. A hole in the middle of a working band is not something a
delay-related mechanism produces — it is intermittency, and V.34 training was
already known to be intermittent (roughly 3 in 5 at one point). **Every sweep
here is one call per point, which cannot distinguish "this value fails" from
"this call failed."**

That undermines more than this table. The V.32 "cliff between 120 and 180",
recorded elsewhere in this file as though settled, rests on the same design and
is reopened as task #92. So does the claim that IODELAY 120 gives a better rate
than 240 — the sweep shows 14400 at 104, 120 and 152 alike, so that comparison
was noise read as signal.

The fix is repeats: N identical calls at one setting to measure the per-call
failure rate first (`testbench/repeat.sh`), then sweeps scored as pass
FRACTIONS. Until that is done, treat every pass/fail cell in this file as one
sample.

#### The baseline, measured — and it is bad enough to invalidate the sweeps

Five IDENTICAL calls, `IODELAY 120`, V.34, nothing changed between them:

```
  1  far=33600/ARQ  blob=14400   PASS
  2  far=33600/ARQ  blob=14400   PASS
  3  far=33600/ARQ  blob=4800    PASS      <- same config, a third of the rate
  4  far=—          no connect   —         <- same config, outright failure
  5  far=33600/ARQ  blob=14400   PASS
  ---
  connected 4/5, data both ways 4/5
```

**One call in five fails at a fixed configuration, and the rate varies by 3x
between successes.** At a 20% per-call failure rate, a six-point sweep has
about a 74% chance of throwing at least one spurious "no connect" — which is
almost certainly the hole at 136, and may be 88, 180 and 240 as well.

So: **every single-sample sweep in this file measures the noise floor, not the
parameter.** Any future sweep needs at least five calls per point, reported as
a fraction. That is roughly ten minutes per point, which is affordable and was
skipped.

What survives, because it does not depend on the sweeps: the unit of IODELAY,
`echo_delay = IODELAY + 60` (confirmed against the trace at four points), the
value of making it configurable, and the far end reporting **33600/ARQ on
every successful call** while our side reports 14400 or worse — consistent
across five calls and two different modems.

### The ladder re-run with T.38 off — and two results MOVED

`ladder.sh` walks one modulation per call, forced on both ends, against the
SupraExpress on 1901. Run with `fax protocol none` AND `playout-delay mode
fixed / nominal 80` in place:

| rung | this run | earlier |
|---|---|---|
| V.22 | **PASS, data both ways** | pass |
| V.22bis | **PASS, data both ways** | pass |
| V.32 | **NO CONNECT** | **PASS at 14400 (row 8)** |
| V.32bis | NO CONNECT | fails (rows 6, 9) |
| V.34 | connects at 12000, data FAIL | **PASS both ways (row 18)** |

**Two cells disagree with earlier measurements, and neither is explained.**
Recorded rather than smoothed over, because a bench whose results move is worth
less than one that admits it:

* **V.32 connected at 14400 in row 8 and does not connect now.** Row 8 predates
  BOTH the fax fix and the jitter change. V.32 has no V.21 phase, so the fax
  setting should be irrelevant to it — which points at the fixed 80 ms playout
  as the thing that changed. That is a plausible mechanism: V.32 runs an echo
  canceller whose delay budget comes from `MDMPRM_IODELAY`, and adding a fixed
  80 ms to the path moves the echo outside the window it was set for. It is a
  hypothesis; nobody has measured it.
* **V.34 passed the data probe in row 18 and failed it here**, on the same
  modem and the same settings but with the SupraExpress capped at V.34
  (`AT+MS=11,1`) instead of left in its V.90 automode. So V.34 data carriage is
  intermittent too, not reliably good.

The stable, repeatable results are V.22 and V.22bis: pass, both ways, every
time. Everything above them is currently intermittent.

Next, in order: sweep `playout-delay nominal` (60 / 80 / 100 / 120, and
adaptive as a control) with V.32 as the probe, since V.32 is the rung that has
demonstrably changed behaviour and is cheap to test. Then revisit
`MDMPRM_IODELAY`, which trades against it directly.

### ROW 18: V.34 AGAINST REAL HARDWARE, DATA BOTH WAYS, PASS

The bar this project set in task #81, met against a physical modem:

    31.29  tty  CONNECT 115200
    31.37  pty  CONNECT 12000
    36.49  pty  FROM-TTY 0123456789 the quick brown fox
    36.60  tty  FROM-PTY 9876543210 the quick brown fox
       tty received the other end's probe: YES
       pty received the other end's probe: YES
    === DATA BOTH WAYS: PASS

`slmodemd` forced to V.34, the SupraExpress 56e PRO in automode, over SIP with
G.711 A-law and no transcode. V.8 completed and chose the datapump itself:
`V8_OK`, `dp_requested=34`, then `modem report result: 1 (CONNECT)`.

**The blob speaks V.34 to a real modem over VoIP and carries data both ways.**

Two things this settles:

* Every SupraExpress failure recorded earlier in this file was the fax relay.
  Nothing was wrong with the modem, and nothing was wrong with the blob.
* The `/ARQ` mismatch that blocks the data probe against the USR Courier (#95)
  is specific to that pairing, not general -- the SupraExpress passes data on
  the same code, same path, same settings.

Configuration in force: `fax protocol none` and `playout-delay mode fixed` /
`nominal 80` on the VG204, `MDMPRM_IODELAY` 240 in slmodemd.

### SOLVED: it was T.38 fax relay on the ATA. V.34 now negotiates.

`voice service voip / fax protocol none` on the VG204 -- one line -- and V.8
completes:

    0  V8_INIT
    6  V8_ORG_WAITING_FOR_ANSAM
    7  V8_ORG_ANSAM_DETECTED_WAITING_TE
    8  V8_ORG_SEND_CM
    10 V8_ORG_SEND_CJ      <-- JM detected at last, CJ sent
    13 V8_OK               <-- dp_requested=34 (DP_V34), delay=864, qc_index=9

and V.34 trains: `CONNECT 14400/ARQ` at the Courier, `CONNECT 14400` at the
blob. The reasoning below stands; this section is the outcome.

**The blob was never at fault.** Every earlier attribution in this file that
leaned toward the reconstruction was wrong, and the reason two independent
modems AND SpanDSP all failed the same way is that all three were being fed a
V.21 exchange the gateway was busy converting to T.38.

#### What is still marginal: V.34 TRAINING, not V.8

Three consecutive V.34 attempts after the fix:

| run | V.8 | V.34 training |
|---|---|---|
| row 12 | OK, DP_V34 | CONNECT 14400/ARQ |
| row 13 | OK, DP_V34 | CONNECT 28800/ARQ (blob reports 12000 -- V.34 permits asymmetric rates) |
| row 14 | OK, DP_V34 | **fails**, NO CARRIER ~20 s after V8_OK |

So V.8 is now reliable, three for three, and the datapump training that follows
it is not. That is a different and later stage, and it is the one where jitter
genuinely matters -- which promotes the `playout-delay` change from "probably
unnecessary" to the next thing to try:

    dial-peer voice 101 voip
     playout-delay mode fixed
     playout-delay nominal 80

The default is adaptive, which inserts and deletes samples to track delay. A
V.22bis link does not care; a V.34 receiver's timing recovery does. This is
local to the VG204 and needs no cooperation from Asterisk or anything else.

`MDMPRM_IODELAY` also becomes worth revisiting, for the first time with
evidence: it was ruled out earlier only because it changed nothing while V.8
was failing -- which it could not have, since the call never reached the
datapump. Now that training is the failing stage, the parameter that sets the
receiver's delay budget is back in scope.

#### A harness bug this exposed, and one it did not

The data probe fails on every ARQ link, INCLUDING row 11, which had no blob in
it at all -- so it is the harness, not the link. Two causes, one fixed:

* Probing too early. `CONNECT` is reported when the DATA PUMP has trained;
  V.42 LAPM negotiation then runs on top, and anything written during that
  window is discarded. The settle time is now 5 s, not 1 s.
* Still open: the Courier's `AT&F` restores `&B0`, "DTE rate follows the line
  rate", so after CONNECT it reconfigures its serial port to the line rate
  while the harness is still at 115200 -- which is exactly what the garbage
  (`bA/.bdu9s`) in the logs is. `AT&B1` pins it, and is now passed, but the run
  that carried it failed to train for unrelated reasons, so the fix is
  UNVERIFIED. The SupraExpress rows never hit this because they never used
  `&F`.

### How it was found: the ATA config (retained for the reasoning)

1901 and 1902 are both FXS ports on one **Cisco VG204**. Its config explains
the shape of the failure better than anything else so far.

**The problem, in `voice service voip`:**

    fax protocol t38 version 0 ls-redundancy 0 hs-redundancy 0 fallback pass-through g711alaw

T.38 fax relay is on. A fax-relay gateway listens for the start of a fax call:
a 2100 Hz answer tone followed by V.21 preamble. **A V.34 call opens with
exactly that** -- ANSam at 2100 Hz, then V.21 FSK carrying CM and JM. At the
detector level the two are the same, and when the gateway decides it is a fax
it stops passing audio through and starts demodulating V.21 into T.38 packets.
The V.8 exchange does not survive that.

Every observation fits:

* V.22, V.22bis and V.32 all pass -- **none of them has a V.21 phase**, so
  nothing triggers the detector.
* V.34 fails immediately after ANSam, precisely at the V.21 CM/JM stage.
* The SupraExpress's JM appears **twice and then stops** -- which is what a
  mid-message cutover to T.38 would look like.
* **Both** the blob and SpanDSP reject that reply. A truncated or re-modulated
  JM explains two independent readers refusing it; "both implementations are
  wrong in the same way" does not.
* Row 11 works because 1901 and 1902 are ports on the SAME VG204: that call is
  hairpinned inside the gateway and need never touch the VoIP fax path at all.

**Also worth changing:**

    dial-peer voice 101 voip
     no modem passthrough          <-- should be: modem passthrough nse codec g711alaw

`modem passthrough` is what tells the gateway "this is a modem, stop treating
it as speech": it suppresses fax relay, VAD and the DSP's speech processing for
the duration. It is off.

And there is **no `playout-delay`** configured, so the jitter buffer is
adaptive by default. An adaptive playout buffer inserts and deletes samples to
track delay, which is precisely what a modem's timing recovery cannot tolerate.
For modem traffic it should be fixed.

Suggested changes (untested -- this is a live gateway and the config was read,
not written):

    voice service voip
     fax protocol none
    !
    dial-peer voice 101 voip
     modem passthrough nse codec g711alaw
     playout-delay mode fixed
     playout-delay nominal 80
     fax rate disable

**What the VG204 already has right**, so it can be ruled out: `codec g711alaw`,
`no vad`, `no echo-cancel enable` on every port, `no comfort-noise`, `no
non-linear`, `compand-type a-law`, `bearer-cap 3100Hz`.

### No transcode: the RTP leg is G.711 A-law end to end

Verified rather than assumed, from row 10's SIP trace:

    m=audio 56493 RTP/AVP 8 0 120        <- payload type 8 (PCMA) offered first
    a=rtpmap:8 PCMA/8000
    Audio updated, stream #0: PCMA (sendrecv)
    #0 audio PCMA @8kHz, sendrecv, peer=10.0.0.26:10084

PCMA is offered first and PCMA is what both ends run, matching the VG204's
`codec g711alaw`, so Asterisk has no reason to transcode. `d-modem` also
disables PLC and VAD on both G.711 codecs explicitly. PCMU is still offered as
a fallback; it is never selected here, but removing it would make a mu-law path
impossible rather than merely unused.

### Where that leaves the attribution

Nothing here is settled, and the honest state is that the evidence now points
in two directions at once:

* **Against the blob**: two unrelated modems fail it identically, and they
  succeed with each other at V.34/28800.
* **Against the far end or the path**: the blob's CM is provably well formed,
  and SpanDSP independently rejects the reply the SupraExpress sends.

The reconciling hypothesis, untested: the JM *is* sent, twice, and something in
the RTP leg -- packet timing, the jitter buffer, or a gap at the start of the
answerer's carrier -- damages or truncates it, so that both the blob and
SpanDSP see something they will not accept, while a direct PBX-internal call
between two analogue ports is unaffected. That would explain every observation
including row 11.

The test for it: run `v8analyse` in `caller` mode against a recording of a call
that WORKS. There is no such recording yet, because row 11 bypassed the blob
and so was never recorded. Getting one means putting the recorder on the RTP
leg while two real modems talk through it.

### Next

Two candidate explanations, and they are distinguishable:

1. **The blob's CM is malformed, or our V.21 channel-1 transmission of it is.**
   The CM decodes off the recording as the eight octets `e0 c1 65 11 10 0d 27
   10` repeating every 300 ms. Check that against V.8's CM encoding and against
   what our own reconstruction (`src/v8/`) builds -- we have an independent
   implementation of the same message, so this is a differential test we can
   run offline, with no calls.
2. **The timing is wrong.** V.8 constrains when CM may start relative to
   ANSam and how long it persists. The recordings have the timing to the
   millisecond.

The strongest offline test available, needing no further calls: feed
`captures/row10-v34-courier.modem_rx.raw` (9600 Hz, the exact samples the blob
was fed) into OUR `V8Process` with `answer=0`, and feed our own CM into our own
answering side. If our reconstruction accepts what the blob sent but the
hardware does not, that narrows it to the wire; if our answering side also
rejects it, the message itself is suspect.

### Rows 1, 3, 4 — not yet run

Row 1 is the SupraExpress originating into `slmodemd`; rows 3 and 4 repeat both
directions against our reconstruction rather than the blob, which needs the
hybrid link (`claude_re` task #89).
