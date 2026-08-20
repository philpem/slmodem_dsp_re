#!/bin/bash
#
# ata_blob_recon_ab.sh -- order-balanced physical ATA comparison.
#
#   OLINET_TTY=/dev/ttyUSB0 ./ata_blob_recon_ab.sh [pairs] [role]
#
# Runs the original SmartLink blob and reconstructed datapump through the same
# guarded PBX/ATA path.  The two arms alternate first position on every pair:
# the radio/ATA/modem may drift during a run, so a blocked all-blob then
# all-reconstruction batch would make time a hidden configuration variable.
#
# The runner records `callstats.py`'s far-end fields, including final (not
# CONNECT) rates and retrain ownership.  It deliberately defaults to a
# no-data hold: establish carrier/retrain stability first; pass HOLD=0 only
# when explicitly testing the short bidirectional payload probe.
#
set -u

BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
ROOT=/home/philpem/dev/sip-D-modem
PAIRS=${1:-1}
ROLE=${2:-olinet}
HOLD=${HOLD:-60}
PREFIX=${PREFIX:-$BENCH/captures/ata-blob-recon}
BLOB=${BLOB:-$ROOT/d-modem/slmodemd/slmodemd}
RECON=${RECON:-$ROOT/claude_re/build/hybrid-fit/slmodemd-fit}

[ "$PAIRS" -ge 1 ] 2>/dev/null || {
	echo "usage: $0 [positive-pairs] [olinet|courier|supra]" >&2
	exit 2
}
for f in "$BLOB" "$RECON"; do
	[ -x "$f" ] || { echo "missing executable: $f" >&2; exit 2; }
done

. "$BENCH/modems.sh"
TTY=$(modem_require "$ROLE") || exit 3
EXT=$(modem_ext "$ROLE") || exit 3
CSV="$PREFIX.csv"
mkdir -p "$(dirname "$PREFIX")"
python3 "$BENCH/callstats.py" --header > "$CSV"

echo "ATA blob/reconstruction A/B: $PAIRS pair(s), $ROLE ext $EXT, hold $HOLD s"
echo "CSV: $CSV"

run_arm() {
	local pair=$1 arm=$2 binary label
	case "$arm" in
	blob) binary=$BLOB ;;
	recon) binary=$RECON ;;
	*) return 2 ;;
	esac
	label="$PREFIX-p${pair}-${arm}"
	echo "=== pair $pair: $arm ($binary)"
	TTY="$ROLE" SLMODEMD="$binary" HOLD="$HOLD" \
		timeout 180 bash "$BENCH/row.sh" "$label" pty "$EXT" \
		> "$label.run.log" 2>&1
	local rc=$?
	python3 "$BENCH/callstats.py" "$label" >> "$CSV"
	printf '  arm=%s exit=%d  ' "$arm" "$rc"
	tail -1 "$CSV"
	return "$rc"
}

for pair in $(seq 1 "$PAIRS"); do
	# Deterministic order balancing: each arm occurs first equally often over
	# every two pairs, while pair one can be rerun exactly from its artifacts.
	if [ $((pair % 2)) -eq 1 ]; then
		first=blob; second=recon
	else
		first=recon; second=blob
	fi
	run_arm "$pair" "$first" || true
	# The modem needs its escape-sequence/ATH guard interval before a new
	# pre-flight.  row.sh retries too, but this keeps the retry from becoming
	# routine and preserves arm order rather than silently skipping a run.
	sleep 6
	run_arm "$pair" "$second" || true
	done

echo "=== completed; inspect $CSV and the per-call .lastlink.log files"
