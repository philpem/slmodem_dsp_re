#!/bin/sh
# Partially link a complete period-toolchain object manifest with binutils 2.15.
set -eu

cd "$(dirname "$0")/../.."

output=${1:-build/partial/dsplibs.o}
manifest=${2:-build/tc_repro/tc_manifest.txt}
image=${TC_IMAGE:-dsplibs-tc342-gentoo}
object_dir=$(dirname "$manifest")

[ -s "$manifest" ] || {
	echo "partiallink: $manifest is missing or empty" >&2
	exit 1
}

expected=$(wc -l < "$manifest")
missing=0
while read -r object source; do
	[ -n "$object" ] || continue
	if [ ! -f "$object_dir/$object" ]; then
		echo "partiallink: missing $object_dir/$object ($source)" >&2
		missing=$((missing + 1))
	fi
done < "$manifest"
[ "$missing" -eq 0 ] || exit 1

docker image inspect "$image" >/dev/null 2>&1 || {
	echo "partiallink: no docker image '$image'" >&2
	exit 1
}
mkdir -p "$(dirname "$output")"

docker run --rm --label dsplibs-partial-link \
	--user "$(id -u):$(id -g)" --platform linux/386 \
	-v "$(pwd):/src" -w /src "$image" sh -c '
	set -eu
	manifest=$1
	object_dir=$2
	output=$3
	set --
	while read object source; do
		[ -n "$object" ] || continue
		set -- "$@" "/src/$object_dir/$object"
	done < "/src/$manifest"
	[ "$#" -gt 0 ]
	exec ld -r -o "/src/$output" "$@"
	' sh "$manifest" "$object_dir" "$output"

echo "partial link: $expected objects from $expected manifest entries"
echo "              binutils 2.15 [$image], wrote $output"
