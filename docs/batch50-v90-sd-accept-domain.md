# SD verdict live through below-limit early return

Count-after-endpoints matches180B except two comparison bytes: current cmp register,memory / JA versus original cmp memory,register / JB. Explicit result0 in the below-limit arm removes the entry verdict entirely. Original below-limit edge returns the still-live entry verdict. Test a single new control with `if(run<limit) return result; result=1;` crossed against count-after-endpoints seed; no comparison-expression permutations. Baseline plus seed plus this one candidate. Close if this observed return edge does not reproduce.
