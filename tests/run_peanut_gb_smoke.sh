#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
smoke_binary=$(mktemp "${TMPDIR:-/tmp}/gametiger-peanut-gb-smoke.XXXXXX")
trap 'rm -f "$smoke_binary"' EXIT HUP INT TERM

"${CC:-cc}" \
	-std=c11 -O1 -g3 -Wall -Wextra -Werror \
	-fsanitize=address,undefined -fno-omit-frame-pointer \
	"$repo_root/tests/peanut_gb_smoke.c" \
	-o "$smoke_binary"

ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1} \
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1} \
	"$smoke_binary" "$@"
