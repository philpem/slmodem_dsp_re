# V92 filter hot arm and fixed copy-loop latch

Base902f47fa. Original tapsFir-positive arm branches to cold both-filters when IIR active; current code falls through both-filters. Original no-filter copy has one12-element loop body and signed <=11 latch; ours literal<12 gets an optimizer-duplicated body. Cross FIR-only inner hot arm with explicit <=11 copy loop. Test ordinary do/while copy as independent loop organization (known12positive). No function/TU order or compiler flags changed. Complete original raw control plus five finite candidates, every caller/bystander audited.
