# calcK log-result use-site narrowing

The log-product-first float local aligns call order but produces90B rather than94: it narrows log10(k) before loading the log2 constant. Blob loads that constant while product logarithm is wide, then narrows for its multiplication operand. Test retaining the native longdouble log10l result until the existing explicit float cast at return; float l2 and reciprocal multiplication unchanged. Cross declaration at first use versus entry. No double intermediate/new rounding boundary. Three cells/raw original completeTU.
