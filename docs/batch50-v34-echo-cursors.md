# V34 echo copy and dot-product cursors

Declared before compilation: raw902f47fa baseline plus four complete TUs on
the independently exact energy-pointer seed, crossing advancing history-copy
cursor with advancing coefficient/history dot-product cursors. Blob advances
history pointer after movzwl-next/store-current in copy loop, rather than
indexed k addressing; dot loop advances both coeff and history immediately
after loads. Original guards/counts, ring subtraction, signed inputs and
arithmetic remain unchanged. No source countdown requested: prior energy
cross shows GCC's loop reversal generates it from ascending source counts.
Five cells, all data/metadata/nontext/relocs and 26 canonical bodies audited;
no register permutations/padding. On misses close domain. Parent period gate
at batch end; no mutation/fuzz/harness runs.

Result: no EchoFilter exact.137/139/138/140B vs139; copy cells additionally change nonexact EchoAdapt. Energy gain retained; no cursor adoption.
