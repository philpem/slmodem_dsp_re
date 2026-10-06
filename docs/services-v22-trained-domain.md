# V22 trained-symbol source loop and final result ownership

Basee0052eec. Original RxTrained1200 has a guarded advancing short loop and
retains narrow count alongside promoted loop bound; the reconstruction
manually peels the first element into an if/do. Test ordinary for guard
(index<count && symbols[index]==3), short index retained.

Original RxTrained2400 decrements backward index even on the mismatching
symbol, and uses shared final >7 threshold. Test ordinary short run while
(run<count && symbols[index--]==15), retaining all accesses and short wrap.
Cross original Boolean expression with explicit branch result as original's
one/zero exits. Eight complete TUs include raw baseline and each independent
axis plus combinations; only these two leaves may be edited. Preserve all
bodies, symbols, data and exact set, no independent raw-register tuning.
