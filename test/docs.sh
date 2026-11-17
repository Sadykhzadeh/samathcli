#!/bin/sh
# Checks that the options --help prints, the manual page (samathcli.1) and the
# options table of README.md name the same flags. Run from `make check`
# with the binary as the argument, or with ./samath by default.

root=$(cd "$(dirname "$0")/.." && pwd)
bin=${1:-$root/samath}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"$bin" --help | grep -E '^ {2,6}-' | sed 's/^ *//; s/  .*//' |
  tr ', ' '\n\n' | grep -E '^--?[a-zA-Z]' | sort -u > "$work/help"

awk '/^\.TP/ { getline; print }' "$root/samathcli.1" |
  sed -e 's/\\f[BIR]//g' -e 's/\\-/-/g' |
  tr ', ' '\n\n' | grep -E '^--?[a-zA-Z]' | sort -u > "$work/man"

grep '^| `-' "$root/README.md" | cut -d'|' -f2 | tr -d '`' |
  tr ', ' '\n\n' | grep -E '^--?[a-zA-Z]' | sort -u > "$work/readme"

if [ ! -s "$work/help" ]; then
  echo "no options found in $bin --help" >&2
  exit 1
fi

status=0
for source in man readme; do
  if ! diff "$work/help" "$work/$source" > "$work/diff"; then
    echo "the options in --help and in the $source differ (< --help, > $source):" >&2
    cat "$work/diff" >&2
    status=1
  fi
done

if [ "$status" = 0 ]; then
  echo "docs: $(wc -l < "$work/help" | tr -d ' ') options agree across --help, samathcli.1 and README.md"
fi
exit "$status"
