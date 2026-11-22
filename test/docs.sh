#!/bin/sh
# Checks that the options --help prints, the manual page (samathcli.1) and the
# options table of README.md name the same flags, that the long options in the
# table of options.c are the ones --help prints, and that the manual page and
# README give the bounds and the default address the code has. Run from
# `make check` with the binary as the argument, or with ./samath by default.

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

grep -o '{"[a-z-]*", '"'"'.'"'"'}' "$root/options.c" | sed 's/^{"\([^"]*\)".*/--\1/' |
  sort -u > "$work/table"
grep -E '^--' "$work/help" | grep -vxE -- '--(help|version)' > "$work/help-table"

status=0
if ! diff "$work/help-table" "$work/table" > "$work/diff"; then
  echo "the long options in options.c and --help differ (< --help, > options.c):" >&2
  cat "$work/diff" >&2
  status=1
fi
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

# The numbers and the default address the manual page and README give, as the
# code has them, each as a whole phrase: "1 to 3600" does not match "1 to 36000".
limit() { sed -n "s/^#define $1 \\([0-9]*\\).*/\\1/p" "$root/options.h" "$root/getQuestion.h"; }
# Fails, and sets status, when the file does not say one of the phrases.
mentions() {
  flat=$(tr -d '\r' < "$1" | tr '\n' ' ' | tr -s ' ')
  echo "$2" | while IFS= read -r phrase; do
    if ! printf '%s\n' "$flat" | grep -qwF -- "$phrase"; then
      echo "$1 does not give \"$phrase\", as the code has it" >&2
      exit 1
    fi
  done || status=1
}
bounds="1 to $(limit TIME_LIMIT_MAX)
1 to $(limit TOTAL_TIME_MAX)
0 to $(limit SEED_MAX)
$(limit LENGTH_MIN) to $(limit LENGTH_MAX)
$(limit MAX_MIN) to $(limit MAX_MAX)
0 to $(limit RANGE_MAX)
at most $(limit RANGE_WIDTH)"
fetching="within $(limit CONNECT_SECONDS) seconds
no answer within $(limit ANSWER_SECONDS)
up to $(limit REDIRECTS_MAX) times
batches of $(limit QUIZ_BATCH_MAX)"
default=$(sed -n 's/^#define apiUrlDefault "\(.*\)"$/\1/p' "$root/getQuestion.h")
mentions "$root/samathcli.1" "$bounds
$default"
mentions "$root/README.md" "$bounds
$default
$fetching"

# --help and --version exit with 0, which the exit status sections of the manual
# page and README have to say.
exit_man=$(sed -n '/^\.SH EXIT STATUS/,/^\.SH /p' "$root/samathcli.1" | tr -d '\r' | sed 's/\\-/-/g')
exit_readme=$(sed -n '/^### Exit status/,/^### /p' "$root/README.md" | tr -d '\r')
for option in --help --version; do
  "$bin" "$option" > /dev/null 2>&1 || { echo "$option does not exit with 0" >&2; status=1; }
  printf '%s\n' "$exit_man" | grep -qF -- "$option" ||
    { echo "the EXIT STATUS of samathcli.1 does not name $option" >&2; status=1; }
  printf '%s\n' "$exit_readme" | grep -qF -- "$option" ||
    { echo "the Exit status of README.md does not name $option" >&2; status=1; }
done

if [ "$status" = 0 ]; then
  echo "docs: $(wc -l < "$work/help" | tr -d ' ') options agree across --help, options.c, samathcli.1 and README.md"
fi
exit "$status"
