# testbench tools

Every tool in this directory, what it is for, and what it needs before it will
tell you anything true. Grouped by what you are trying to do, because that is
how you will be looking.

`README.md` is the bench itself — the safety rules, the modems, the findings.
**Read its Safety section before running anything in the first group below.**

Legend used throughout:

| mark | meaning |
|---|---|
| **DIALS** | places a real call through the PBX. The PBX reaches the PSTN. |
| **HW** | opens a hardware modem's serial port |
| **EMU** | emulated only — no PBX, no hardware, no dial string can escape |
| **BRANCH** | needs a build from `v34-instrumentation` to gather *new* data; reads archived logs fine on master |
| **LIB** | imported by other tools, not run directly |

---

## 1. Placing calls

### Hardware, through the PBX — **DIALS**

| tool | what it does |
|---|---|
| `row.sh` | One row of the hardware interop matrix. The workhorse: one call, both ends logged, provenance written in. |
| `row2.sh`, `row2-fork.sh` | Matrix row 2 — d-modem + blob originating, SupraExpress answering. The `-fork` variant drives the fork's binaries. |
| `dial.sh` | One D-Modem call to the lab modem, with a hard destination guard. The low-level dialler. |
| `batch.sh` | N calls at one fixed configuration, one CSV row each (task #108). |
| `repeat.sh` | Repeatability baseline: the same configuration, N times. The control every A/B needs. |
| `ladder.sh` | Walk the modulation ladder, one call per rung, and print a summary. |
| `holdlong.sh` | Hold a call open and watch whether the rate keeps climbing. **HW** |
| `retrain-test.sh` | Paired test: rate at first CONNECT vs rate after a retrain (task #109). |
| `hw2hw.sh` | Control experiment — one physical modem dials the other across the same PBX, with no software of ours in the path. |
| `relaycall.sh` | The control for finding 1466: two hardware modems over the SIP path. |
| `asymmetry.sh` | Is the 33600/14400 split an asymmetric line, or a fault in our receiver? |
| `echo-compare.sh` | Does the echo — and the receive deficit it explains — depend on which modem answers? |
| `iodelay-sweep.sh` | Sweep `MDMPRM_IODELAY` against a fixed modulation to find the operating point. |
| `rxrate-sweep.sh` | Does our receive rate move with IODELAY? (task #104) |
| `v34-rate-sweep.sh` | Sweep IODELAY across the usable band, scored on connect *rate*, not connect. |
| `v32-repeat-sweep.sh` | The V.32 IODELAY sweep, redone with repeats. |
| `call.py` | Drives both ends of a test call and logs both, continuously. **LIB** in practice — the shell harnesses call it. **HW** |

### Emulated — no hardware, no PBX — **EMU**

`chanshim.py` ignores the dial string entirely, so no destination guard is
involved and none can be. These do not need a quiet machine: both ends are
self-paced over blocking sockets.

| tool | what it does |
|---|---|
| `chancall.sh` | One emulated call: two of our datapumps, one channel, no hardware. **BRANCH** |
| `hsfcall.sh` | One emulated call: our datapump against the Conexant HSF. **BRANCH** |
| `chanshim.py` | The channel model — runs in d-modem's place for each end and joins them over loopback. Band limit, delay, noise, loss, `CHAN_SLIP`. Carries selectable line models under `CHAN_LINE_MODEL`. |
| `hsfshim.py` | Puts the HSF datapump on the far end of `chanshim.py`: socketpairs, ring generation, framed relay. |
| `ladder.py` | The impairment ladder — sweeps `CHAN_SLIP` or `CHAN_LOSS` across both peers and reports median rate with connect fraction. |
| `replay.py` | Feed a recorded call into slmodemd in d-modem's place. Open loop: the far end cannot react. |
| `replaydte.py` | Act as the DTE for a replayed call. |
| `replaycmp.sh` | Run two datapumps over the same recorded audio and diff their behaviour. |

### Relays

| tool | what it does |
|---|---|
| `relay.py` | A SIP back-to-back user agent, so two hardware modems can talk through the path. |
| `rtprelay.py` | A back-to-back UA that forwards RTP packets untouched. |

---

## 2. Safety, devices and pre-flight

**These are the guards. Do not weaken them to make something run.**

| tool | what it does |
|---|---|
| `dmodem-guard.sh`, `dmodem-guard-fork.sh` | slmodemd's `-e` target. Checks the dial string arriving in `argv`, exact match after stripping dial modifiers. The `-fork` variant is the one the current harness uses. |
| `waitquiet.sh` | Block until the machine is quiet enough to measure on. Finding 1951: a call at load 4.28 returned CONNECT 4800 against 31200 quiet. **Call it in front of a batch — the tools do not gate themselves.** |
| `modems.sh` | Resolve a modem ROLE (`supra`/`courier`/`olinet`) to a stable device path, and refuse to dial without one. `ttyUSBn` is not a stable name. **HW** |
| `atprobe.py` | Is there a modem behind this device node, and which one? Pre-flight; exit 3 is distinct from the allow-list's exit 2. **HW** |
| `modemid.py` | Ask a modem what it is, and derive its AT dialect from that. **HW** |
| `lastlink.py` | Ask a modem what its last connection actually was. **HW** |

---

## 3. Reducing one call

| tool | what it does |
|---|---|
| `callstats.py` | Reduce one call's logs to a single CSV row. The unit every batch tool consumes. |
| `abextract.py` | Pull the rate decision out of a run's logs, correctly. |
| `audiostats.py` | Signal levels for a call, robust to relay clicks. |
| `g711level.py` | What the 8-bit codec on the SIP path actually costs us. |
| `v21decode.py` | Decode the V.21 FSK traffic out of a recorded call. **LIB** |
| `capture_io.py` | Load a capture's audio. **LIB** |
| `archive.py` | Copy the captures worth keeping to the ZFS store, and index them. |

---

## 4. Comparing batches

| tool | what it does |
|---|---|
| `batchcompare.py` | Did the rate distribution actually move between two batches? |
| `batchanalyse.py` | Test covariates against a sample that can reject one. |
| `handshakeorder.py` | The rate decision, broken out by **which** handshake it is (findings 3203/3204). |
| `snrblocks.py` | The receiver's own SNR at the moment it chooses a rate. |
| `ratepenalty.py` | Did the −2 rate penalty fire, and on which handshakes? (finding 6900) **BRANCH** |
| `jbtiming.py` | Is the jitter buffer's insertion rate lower during training? (finding 6903) Reads archived `JBSTAT`; that instrumentation no longer exists in `d-modem.c`, so the archive is all there is. |
| `freezescan.py` | Did a frozen equaliser manufacture the retrain? **BRANCH** |

---

## 5. Characterising the channel

| tool | what it does |
|---|---|
| `linesweep.py` | **Measure, store, model, compare and plot the line response under different ATA impedance settings, per modem.** `ingest` from existing logs, `measure` places the calls, `list`/`compare`/`stats`/`plot` read the store in `linemodels/`. See §7. **DIALS** (only `measure`) |
| `bandshape.py` | The channel's magnitude response and in-band tilt from the V.34 line probe, scored against the curve the emulator hardcodes. The reduction engine `linesweep` imports. **BRANCH** |
| `probeplot.py` | The V.34 line probe's measured spectrum, drawn. **BRANCH** |
| `chirpdelay.py` | Round-trip delay, measured rather than inferred. |
| `linesim.py` | Score tilt estimators against a simulated subscriber loop. |
| `fitplot.py` | Least squares against Theil–Sen, drawn rather than tabulated. **BRANCH** |

---

## 6. Echo

| tool | what it does |
|---|---|
| `echoscan.py` | Is our own transmit present in our own receive, and at what lag? `--selftest` plants a known ladder and checks it comes back. **This is the one to use.** |
| `linesweep.py` | Also measures echo, per impedance arm, via `echoscan` — see §5. Terminating impedance IS the hybrid balance network, so echo is the mechanism and bandwidth is the side effect. |
| `echoratio.py` | How much of what we receive is our own transmit coming back? **Its lag search caps at 120 ms**, and the echo on this path is at 171.5 ms — it produced finding 1971's null because it could not see that far. Kept because archived numbers reproduce; it warns on stderr. |
| `echofit.py` | One line of echo numbers for a call, for tabulating. |

---

## 7. Pre-emphasis

| tool | what it does |
|---|---|
| `preemphshape.py` | Pick a V.34 pre-emphasis index by **shape**, not by tilt. The shape matcher (findings 1960/1961). **BRANCH** |
| `test_preempshape.py` | Regression test for the selector — identity, noise robustness, interferer rejection. |
| `preemph_ab.sh` | Does making pre-emphasis index 0 reachable change the link? **DIALS** |
| `preemph_ab2.sh` | The pre-emphasis A/B, run 2. **DIALS HW** |
| `abcompare.py` | The pre-emphasis A/B, analysed exactly as pre-registered. |
| `ab149scan.py` | Score the #149 A/B exactly as `records/ab149-PREREG.txt` says. |

---

## Not here: the instrumentation A/Bs

`ab149.sh` and `preemph_fit_ab.sh` are on the **`v34-instrumentation`** branch,
not on master. Each is an A/B whose two arms differ only by a `dsplib_v34_*`
flag, and master's datapump does not carry those flags — run here they would
set an environment variable nothing reads and report one arm run twice as two.
`tools/benchflags.c` and `tools/hybrid_link.sh` are there for the same reason.

**How to tell, rather than guess:** `probe_flag_for <binary>` in `modems.sh`
answers from the binary itself, and `linesweep.py measure` pre-flights it and
refuses to dial at all if the probe dump is absent. The deployed
`build/hybrid-fit/slmodemd-fit` predates the branch split and still carries it,
so "am I on master?" is the wrong question — ask the artefact.

That is also what **BRANCH** means above: those tools read a debug dump only an
instrumented build emits. They work on the archive from anywhere; to gather
*new* data, build from that branch.

---

## Worked example: comparing ATA impedance settings

```sh
# 1. What is already stored?
python3 linesweep.py list

# 2. Bring in an arm from logs that already exist, tying the impedance to a
#    config dump so it records as EVIDENCED rather than your word for it.
python3 linesweep.py ingest --impedance 600r --modem courier \
    --ata-config /srv/tftp/incoming/vg204-runconf \
    ../testbench/captures/*courier*.slmodemd.log

# 3. Change the ATA, then measure a new arm. linesweep does NOT wait for a
#    quiet machine -- that is your call, so put waitquiet in front of it.
./waitquiet.sh && python3 linesweep.py measure \
    --impedance complex1 --modem courier --calls 10 \
    --ata-config /srv/tftp/incoming/vg204-runconf-new

# 4. All the numbers, then the picture.
python3 linesweep.py stats
python3 linesweep.py plot -o linemodels/compare.png
```

**Which end:** our side is digital — slmodemd to d-modem over a socket, then
RTP — so the hybrid the impedance configures is the far end's analogue drop.
The response measured is far-end-to-us (the direction our receiver equalises),
and the echo is our own transmit returning to our own receiver off that hybrid.
Both are the end that matters; what is not measured is how our transmit lands
at the far end's receiver, which governs the rate *it* asks for. Digital
termination (#110) removes the hybrid altogether, and finding 6901 measured
that case at 33600 with no retrains.

Each arm carries its **echo** as well as its response — median lag and level
from `echoscan`, with a detections/attempted split, because an arm where 3 of
10 calls showed an echo and one where 10 of 10 did are different findings. A
peak that does not stand 4x above the estimator's own noise floor is counted as
a non-detection, not averaged in as a very quiet echo.

Two things that tool will not do, on purpose: it will not treat a bin floored
on more than half the probes as a reading (stored null, printed `--`, and the
plot breaks the line), and it will not extrapolate beyond the measured points.

---

## Keeping this file honest

Every tool in this directory should appear above exactly once. To check:

```sh
for f in *.py *.sh; do grep -q "\`$f\`" TOOLS.md || echo "UNDOCUMENTED: $f"; done
```

A tool added without a line here is a tool the next person will not find.
