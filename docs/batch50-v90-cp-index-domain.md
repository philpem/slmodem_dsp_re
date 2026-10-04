# V92 encoder count initialization lifetime

Previous byte weight capture recovers125B and original x87/table use, leaving setup/parameter-register differences: original clears loop index before FABS and pointer offset, current initializes after these. Cross exact cached-weight and switch representation with index initialized at mode-arm entry (shared local), then independent mode-local counter scope. Three variants plus raw original control. Pointer/weight arithmetic remains unchanged; original loop32-bit index width preserved. Do not reorder unrelated operations or fit register names.
