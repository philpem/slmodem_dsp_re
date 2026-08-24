#!/bin/bash
# Score a #110 A/B batch from its CAPTURES, not from the runner's summary.
#
# Both of the runner's columns were wrong once, in the same way: they took the
# first number that matched rather than the field they named.  `CONNECT` picked
# the FAR END's report (a line rate on the Courier, a serial speed on the
# Supra), and `equerr` picked the LOG TIMESTAMP.  Every value either produced
# was a plausible modem number, so nothing looked wrong.
#
# Scoring from the logs afterwards means a fix costs a re-read, never a re-dial.
set -u
A=${A:-/home/philpem/dev/sip-D-modem/claude_re/testbench/captures}
P=${1:?usage: dtscore.sh LABEL-PREFIX   e.g. dtb or dto}
printf '%-4s %-4s %-7s %-7s %-8s %-4s %-4s\n' i arm ours far equerr hs rtn
for i in $(seq 1 20); do for arm in off on; do
  l=$A/$P-$arm-$i.slmodemd.log; c=$A/$P-$arm-$i.call.log
  [ -f "$l" ] || continue
  ours=$(grep -aoE 'pty << CONNECT[ /]*[0-9]+' "$c" 2>/dev/null | head -1 | grep -oE '[0-9]+$')
  far=$(grep -aoE 'tty << CONNECT[ /]*[0-9]+'  "$c" 2>/dev/null | head -1 | grep -oE '[0-9]+$')
  eq=$(grep -a 'V34DATARATE, equerr' "$l" | tail -1 | grep -oE 'equerr = [0-9]+' | grep -oE '[0-9]+')
  hs=$(grep -ac 'V34DATARATE, automatic' "$l")
  rt=$(grep -ac 'V34TIMING,Retrain started' "$l")
  printf '%-4s %-4s %-7s %-7s %-8s %-4s %-4s\n' "$i" "$arm" "${ours:-none}" "${far:-none}" "${eq:-na}" "$hs" "$rt"
done; done
echo
python3 - "$A" "$P" <<'PY'
import glob,re,sys,statistics as st
A,P=sys.argv[1],sys.argv[2]
def val(pat,txt,grp=1):
    m=re.search(pat,txt); return int(m.group(grp)) if m else None
rows={'off':[],'on':[]}
for arm in ('off','on'):
    for l in sorted(glob.glob(f'{A}/{P}-{arm}-*.slmodemd.log')):
        c=l.replace('.slmodemd.log','.call.log')
        try: L=open(l,errors='replace').read(); C=open(c,errors='replace').read()
        except OSError: continue
        rows[arm].append({
          'ours': val(r'pty << CONNECT[ /]*(\d+)',C),
          'eq':   [int(x) for x in re.findall(r'equerr = (\d+)',L)][-1:] or [None],
          'hs':   len(re.findall(r'V34DATARATE, automatic',L)),
          'rtn':  len(re.findall(r'V34TIMING,Retrain started',L))})
print(f'  {"arm":<5}{"n":>3}{"our rate median":>17}{"equerr median":>15}{"hs":>6}{"retrain":>9}')
for arm in ('off','on'):
    r=rows[arm]
    rate=[x['ours'] for x in r if x['ours']]
    # 32767 is a saturated short, not a measurement -- excluded and counted.
    eq=[x['eq'][0] for x in r if x['eq'][0] not in (None,32767)]
    sat=sum(1 for x in r if x['eq'][0]==32767)
    print(f'  {arm:<5}{len(r):>3}{(st.median(rate) if rate else 0):>17.0f}'
          f'{(st.median(eq) if eq else 0):>15.0f}'
          f'{st.median([x["hs"] for x in r]) if r else 0:>6.1f}'
          f'{st.median([x["rtn"] for x in r]) if r else 0:>9.1f}'
          + (f'   ({sat} equerr saturated at 32767, excluded)' if sat else ''))
print('\n  connected: ' + ', '.join(
    f'{a} {sum(1 for x in rows[a] if x["ours"])}/{len(rows[a])}' for a in ('off','on')))
PY
