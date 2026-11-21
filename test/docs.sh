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

awk '/^\.TP/ { getline; print }' "$root/samathcli.1" | tr -d '\r' |
  sed -e 's/\\f[BIR]//g' -e 's/\\-/-/g' |
  tr ', ' '\n\n' | grep -E '^--?[a-zA-Z]' | sort -u > "$work/man"

grep '^| `-' "$root/README.md" | tr -d '\r' | cut -d'|' -f2 | tr -d '`' |
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

version=$("$bin" --version) || exit 1
if ! grep '^\.TH ' "$root/samathcli.1" | tr -d '\r' | grep -qF "\"$version\""; then
  echo "the .TH line of samathcli.1 does not name \"$version\", as --version prints" >&2
  status=1
fi

# The bounds the manual page and README give for each option, as options.h
# sets them, each as a whole phrase: "1 to 3600" does not match "1 to 36000".
limit() { sed -n "s/^#define $1 \\([0-9]*\\).*/\\1/p" "$root/options.h"; }
bounds="1 to $(limit TIME_LIMIT_MAX)
1 to $(limit TOTAL_TIME_MAX)
0 to $(limit SEED_MAX)
$(limit LENGTH_MIN) to $(limit LENGTH_MAX)
$(limit MAX_MIN) to $(limit MAX_MAX)
0 to $(limit RANGE_MAX)
at most $(limit RANGE_WIDTH)"
for source in samathcli.1 README.md; do
  flat=$(tr -d '\r' < "$root/$source" | tr '\n' ' ' | tr -s ' ')
  echo "$bounds" | while IFS= read -r phrase; do
    if ! printf '%s\n' "$flat" | grep -qwF -- "$phrase"; then
      echo "$source does not give the bound \"$phrase\", as options.h has it" >&2
      exit 1
    fi
  done || status=1
done

if [ "$status" = 0 ]; then
  echo "docs: $(wc -l < "$work/help" | tr -d ' ') options agree across --help, samathcli.1 and README.md"
fi
exit "$status"
