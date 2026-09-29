#!/bin/sh
set -e
patcher="$1"; shift
output=""; objects=""; previous=""
for argument in "$@"; do
    if [ "$previous" = "-o" ]; then output="$argument"; fi
    case "$argument" in *.o) objects="$objects $argument";; esac
    previous="$argument"
done
if [ -z "$output" ] || [ -z "$objects" ]; then echo "link-with-nid-aliases: cannot find the output or object files" >&2; exit 1; fi
library=$(basename "$output"); library="${library%.*}"
work="$output.nid"; mkdir -p "$work"
nm -gU -j $objects | grep -v ":$" | sort -u > "$work/names.txt"
exclusions=""
libc="$(dirname "$output")/libc.prx"
if [ "$library" != "libc" ] && [ -f "$libc" ]; then
    nm -gU -j "$libc" | sort -u > "$work/libc-names.txt"
    exclusions="--preserve-export-names $work/libc-names.txt"
fi
"$patcher" "$library" --alias-list "$work/names.txt" "$work/aliases.txt" $exclusions > /dev/null
exec "$@" -Wl,-alias_list,"$work/aliases.txt"
