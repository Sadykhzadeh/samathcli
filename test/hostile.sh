#!/bin/sh
# Runs samath against hostile command lines and hostile servers
# (test/hostile_server.py). Each run has to exit with status 1, and its
# standard error has to hold only what samath says itself: no sanitizer
# report and no curl or Python noise. `make check` runs it with the binary
# it has just built, which is the sanitizer build under the CI's CFLAGS.
#
# usage: sh test/hostile.sh [./samath]

root=$(cd "$(dirname "$0")/.." && pwd)
bin=${1:-$root/samath}
work=$(mktemp -d)
server=
trap 'test -n "$server" && kill "$server" 2>/dev/null; rm -rf "$work"' EXIT
passed=0
failed=0

if command -v python3 >/dev/null 2>&1; then
  python3 "$root/test/hostile_server.py" > "$work/port" 2>/dev/null &
  server=$!
  waited=0
  while [ ! -s "$work/port" ] && [ "$waited" -lt 50 ]; do
    sleep 0.1
    waited=$((waited + 1))
  done
  base="http://127.0.0.1:$(cat "$work/port")"
else
  echo "python3 not found: the hostile server runs were made against a closed port"
  base=http://127.0.0.1:9
fi
gen="$base/gen"
long=$(printf '%5000s' '' | tr ' ' a)

# run NAME URL ARGS... : runs samath with URL as SAMATH_API_URL and checks it.
run() {
  name=$1
  url=$2
  shift 2
  printf 'A\nB\nC\nD\nA\n' | SAMATH_API_URL="$url" timeout 30 "$bin" "$@" > "$work/out" 2> "$work/err"
  status=$?
  noise=$(grep -E '^curl|curl: \(|Sanitizer|runtime error|Failure writing|^==[0-9]+==|Traceback|python' "$work/err")
  controls=$(LC_ALL=C tr -d '\n' < "$work/err" | LC_ALL=C grep '[[:cntrl:]]')
  said=yes
  if [ -n "$expect" ] && ! grep -q "$expect" "$work/err"; then said=no; fi
  if [ "$status" = 1 ] && [ -z "$noise" ] && [ -z "$controls" ] && [ "$said" = yes ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL: $name: status $status, stderr: $(head -c 200 "$work/err" | tr '\n' ' ' | cat -v) $noise"
  fi
}

run "a very long argument" "$gen" "$long"
run "an empty argument" "$gen" ""
run "an empty length" "$gen" -l ""
run "an empty range" "$gen" -r ""
run "a range that is not numbers" "$gen" -r abc
run "a reversed range" "$gen" -r 5-1
run "a range with a huge end" "$gen" -r 1-99999999999999999999
run "a huge number of questions" "$gen" 99999999999999999999
run "a huge time limit" "$gen" -t 99999999999999 2
run "a negative seed" "$gen" -s -1
run "a seed past 32 bits" "$gen" -s 4294967296
run "a max of zero" "$gen" -m 0 1
run "a negative max" "$gen" -m -5 1
run "empty operators" "$gen" -o "" 1
run "operators with an empty name" "$gen" -o add,,subtract 1
run "an unknown operator" "$gen" -o foo 1
run "a length of one" "$gen" -l 1 1
run "a length of thirteen" "$gen" -l 13 1
run "a negative number after --" "$gen" -- -1
run "a missing time limit" "$gen" -pt
run "a misspelt long option" "$gen" --practise 1
run "an unknown short option" "$gen" -Z 1
run "a control character in an unknown long option" "$gen" "--$(printf 'x\033[2J')" 1
run "a control character in an unknown short option" "$gen" "-$(printf '\033')" 1
run "a control character in a seed" "$gen" -s "$(printf '\r\033[31m')" 1
run "a control character after an option that needs a value" "$gen" -t "$(printf '\033[1m')" 1
run "a control character in an extra number" "$gen" 1 "$(printf '\033[2J')" 1
expect='not "5?"'
run "a raw C1 byte in an argument shows it as ?" "$gen" "5$(printf '\233')" 1
expect='not "5é?"'
run "bytes that are not UTF-8 in an argument show as ?" "$gen" "5é$(printf '\377')" 1
expect='not "5é"'
run "well-formed UTF-8 in an argument is kept" "$gen" "5é" 1
expect=
run "zero questions" "$gen" 0
run "a zero time limit" "$gen" -t 0 1
run "an unreachable server" "http://127.0.0.1:1/gen" 1
run "a server sending garbage" "$base/garbage" 2
expect="more than 1 MB"
run "a server sending too much" "$base/huge" 2
expect=
run "a server that is slow" "$base/slow" 1
run "a server that fails" "$base/broken" 2
run "a server sending bytes that are not UTF-8" "$base/nonutf8" 1
run "a worked answer cut short" "$base/json-cut" -w 3
run "a worked answer nested too deep" "$base/json-deep" -w 3
run "a worked answer that is not tasks" "$base/json-other" -w 1
run "a worked answer with escapes in fields it does not use" "$base/json-escapes" -w 1
expect='It said: bad "x"'
run "a worked refusal shows its message with the escapes read" "$base/json-refusal" -w 1
expect=
expect="more than 1 MB"
run "a worked answer sending too much" "$base/json-huge" -w 2
expect=
run "a refusal with escapes and a long text" "$base/refuse" 1
expect="It said: ?31mBAD ? ok"
run "a refusal with raw C1 and invalid bytes shows them as ?" "$base/refused-bytes" 1
expect="It said: 31mBAD$"
run "a refusal with a UTF-8 C1 control drops it" "$base/refused-c1" 1
expect="It said: BAD ok$"
run "a refusal with bidi overrides drops them" "$base/refused-bidi" 1
expect="It said: 😔 café ?? ??? ???? ???? ??? ?? end$"
run "a refusal keeps well-formed UTF-8 and shows a ? for each byte of the rest" "$base/refused-utf8" 1
expect=
run "a server sending nothing" "$base/empty" 1
run "a file URL" "file:///etc/passwd" 1
run "a dash as the URL" "-" 1
run "a range of one answer" "$gen" -r 0-0 1

echo "hostile: $passed passed, $failed failed"
test "$failed" = 0
