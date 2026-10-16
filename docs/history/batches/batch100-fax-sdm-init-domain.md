# SDMv27 initializer fallthrough

856c1ecb complete V27_SDM.c; two cells only. Original 9a85a compares cfg->nbits against2 and JE jumps to3-bitmask stores; non-two falls through7-mask stores. Current positive arm2 falls through3-mask. Use existing arm-order lever with original predicate/operands/constants/store/read order/null defect fixed. No definition ordering/scratch controls. Full TU all functions/data/exports/nontext/relocs, raw baseline and retained flags. No new family if miss.
