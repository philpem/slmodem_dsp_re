#!/bin/sh
# Fingerprint the code-bearing parts of one object, ignoring debug line info.
#
# A comment pass moves every line number, so `-g` guarantees .debug_line
# differs; hashing the whole object cannot answer "did the code change".
# What must be identical is the emitted sections, the relocations against
# them, and the symbol table.  Validated by re-introducing a known change
# and watching it fire (CLAUDE.md, finding F134).
#
#   tools/objsnap.sh build/src/pump/v34/v34hshak.o
#
# That path exists after `make coverage`; a plain `make` builds
# build/repro/pump/v34/v34hshak.o instead, which is the same source with
# -DDSPLIB_REPRODUCE_BUGS and will not have the same hash.  Findings F3055/3110.
o="$1"
for s in .text .rodata .rodata.str1.1 .rodata.str1.4 .data .bss; do
	objcopy -O binary --only-section=$s "$o" /tmp/objsnap.bin 2>/dev/null
	printf '%s %s\n' "$s" "$(sha256sum < /tmp/objsnap.bin | cut -d' ' -f1)"
done
#
# Relocations against CODE AND DATA only, and the header line dropped: it
# carries the section's file offset, which moves when .debug_line grows, so
# hashing the raw `readelf -rW` reports every comment as a change.  The 656
# .rel.text and 223 .rel.rodata ENTRIES are what a codegen change would move.
#
printf 'reloc %s\n' "$(readelf -rW "$o" |
	awk '/^Relocation section/ { keep = ($0 ~ /\.rel\.(text|rodata|data)/); next }
	     keep' | sha256sum | cut -d' ' -f1)"
printf 'syms  %s\n' "$(readelf -sW "$o" | sha256sum | cut -d' ' -f1)"
