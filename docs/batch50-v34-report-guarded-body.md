# V34 guarded scan body

The explicit guard control emitted test n but retained a second cmp k,n entry,
so it does not recover the blob's single guard. New distinct loop structure:
k=0; if(n>0) do { if(coeff[k]!=0) goto coefficients; } while(++k<n).
Anchored by reference test n; conditional entry followed by body and one latch
cmp, with no second initial cmp. Three complete TU cells: raw902, found-edge
exclusive-bound seed, guarded-do body. Preserve negative/zero counts and every
print/getter boundary. Initial/latch control structure is the question, not
register choice. One cell then close, full TU audit; parent batch phase,
no fuzz/mutation/harness runs.

Result: guarded body reproduces V34EchoReportCoeff EXACT223B. Combined with
energy cursor, full TU gains two symbols (+276 exact bytes),14/26→16/26.
No losses or changed nonexact bystanders. All metadata/named data/nontext
relocation targets preserved, format-string sections retain exact length
and NUL-delimited string multiplicities despite emission-order changes.
Raw complete baseline reproduces under the actual retained profile.
