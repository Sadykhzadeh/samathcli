#!/bin/sh
# Runs the samath binary the way a person would, against a stand-in for curl
# (test/fakecurl.sh), and checks what it prints and what it asks the server.
# Needs ./samath built; `make check` does that first.

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

mkdir "$work/bin" "$work/cwd"
cp "$root/test/fakecurl.sh" "$work/bin/curl"
chmod +x "$work/bin/curl"
PATH="$work/bin:$PATH"
export PATH
CURL_LOG="$work/requests"
export CURL_LOG
default_api="http://api.test/gen?c=true"
SAMATH_API_URL=$default_api
export SAMATH_API_URL

passed=0
failed=0

fail() {
  failed=$((failed + 1))
  echo "FAIL: $name: $1"
}

ok() { passed=$((passed + 1)); }

# run NAME ARGS... : runs samath in an empty directory with $input on stdin,
# leaving stdout in $out, stderr in $err, the exit status in $status and the
# URLs it asked for in $requests.
run() {
  name=$1
  shift
  rm -f "$CURL_LOG" "$CURL_LOG.served"
  : > "$CURL_LOG"
  (cd "$work/cwd" && printf '%s' "$input" | "$root/samath" "$@" > "$work/out" 2> "$work/err")
  status=$?
  out=$(cat "$work/out")
  err=$(cat "$work/err")
  requests=$(wc -l < "$CURL_LOG" | tr -d ' ')
}

# run_at URL NAME ARGS... : the same, against another SAMATH_API_URL.
run_at() {
  SAMATH_API_URL=$1
  shift
  run "$@"
  SAMATH_API_URL=$default_api
}

expect_status() {
  if [ "$status" = "$1" ]; then ok; else fail "exit status $status, wanted $1"; fi
}

expect_requests() {
  if [ "$requests" = "$1" ]; then ok; else fail "$requests requests, wanted $1"; fi
}

expect_out() {
  case "$out" in *"$1"*) ok ;; *) fail "output lacks \"$1\"" ;; esac
}

expect_no_out() {
  case "$out" in *"$1"*) fail "output has \"$1\"" ;; *) ok ;; esac
}

expect_err() {
  case "$err" in *"$1"*) ok ;; *) fail "stderr lacks \"$1\"" ;; esac
}

expect_url() {
  if grep -qF -- "$1" "$CURL_LOG"; then ok; else fail "no request for $1"; fi
}

# The right letters for questions 1..$1: A B C D A B C D ...
right() {
  i=0
  while [ "$i" -lt "$1" ]; do
    case $((i % 4)) in 0) echo a ;; 1) echo b ;; 2) echo c ;; 3) echo D ;; esac
    i=$((i + 1))
  done
}

input=$(right 10)
run "a quiz of ten is one request" 10
expect_status 0
expect_requests 1
expect_url "http://api.test/gen?c=true&count=10"
expect_out "Fantastic"
expect_no_out "You have"

input=$(right 1)
run "one question is the plain request" 1
expect_status 0
expect_requests 1
if [ "$(cat "$CURL_LOG")" = "http://api.test/gen?c=true" ]; then ok; else fail "asked for $(cat "$CURL_LOG")"; fi
expect_out "Fantastic"

input=$(right 250)
run "more than a hundred questions come in batches of a hundred" 250
expect_status 0
expect_requests 3
expect_url "count=100"
expect_url "count=50"
expect_out "Fantastic"

input=$(printf 'b\nb\nb\nb\n')
run "wrong answers are listed with the right ones" 4
expect_status 0
expect_out "You have 3 mistakes!"
expect_out "Task 1. 0+1"
expect_out "Task 3. 2+1"
expect_out "Task 4. 3+1"
expect_no_out "Task 2."
expect_out "1 of 4 right (25%)"

input=$(right 3)
FAKE_IGNORE_COUNT=1
export FAKE_IGNORE_COUNT
run "a server that ignores count is asked one question at a time" 3
unset FAKE_IGNORE_COUNT
expect_status 0
expect_requests 3
expect_url "count=3"
expect_out "Fantastic"

input=$(right 3)
run_at "http://api.test/refuse-count?c=true" "a server that refuses count gets plain requests" 3
expect_status 0
expect_requests 4
expect_out "Fantastic"

input=$(right 3)
run_at "http://api.test/down?c=true" "an unreachable server is one clear message" 3
expect_status 1
expect_err "could not reach SaMathAPI"
expect_no_out "Fantastic"

input=$(right 3)
run_at "http://api.test/garbage?c=true" "a reply that is not a question is the same message" 3
expect_status 1
expect_err "could not reach SaMathAPI"

input=$(right 1)
run "input that ends early ends the quiz, not the server" 3
expect_status 0
expect_requests 1
expect_out "Fantastic"
case "$err" in *"could not reach"*) fail "blamed the server" ;; *) ok ;; esac

input=$(printf 'abcD')
run "an answer is one letter, no Enter" 4
expect_status 0
expect_out "Fantastic"
expect_out "4 of 4 right (100%)"

input=$(printf 'x\n 1 ??\n a\n\tb\n')
run "keys that are not answers are ignored" 2
expect_status 0
expect_out "Fantastic"

# Arrow keys end in the letters A to D, which are answers if let through.
input=$(printf 'a\033[Ab\033OB')
run "arrow keys are not answers" 2
expect_status 0
expect_out "Fantastic"

input=$(printf 'abq')
run "q stops the quiz and shows the result" 5
expect_status 0
expect_out "You stopped after 2 of 5 questions."
expect_out "2 of 2 right (100%)"
expect_out "Fantastic"

input=$(printf 'a\004')
run "Ctrl-D stops it too" 3
expect_status 0
expect_out "You stopped after 1 of 3 questions."

input=$(printf 'q')
run "stopping at once is not a perfect score" 3
expect_status 0
expect_out "You stopped after 0 of 3 questions."
expect_no_out "Fantastic"
expect_no_out "right ("
expect_no_out "You have"

input=$(right 5)
run "nothing is left behind in the directory" 5
if [ -z "$(ls -A "$work/cwd")" ]; then ok; else fail "left $(ls -A "$work/cwd")"; fi

input=$(right 1)
run_at "http://api.test/gen?c=true&x='\$(touch pwned)';touch pwned2" "the URL is never read by a shell" 1
if [ ! -e "$work/cwd/pwned" ] && [ ! -e "$work/cwd/pwned2" ]; then ok; else fail "a command in the URL ran"; fi
expect_url "\$(touch pwned)"

input=
run "no arguments is a hint"
expect_status 0
expect_requests 0
expect_out "provide me a number"

for bad in 0 -3 abc 2.5 ""; do
  input=
  run "\"$bad\" is not a number of questions" "$bad"
  expect_status 1
  expect_requests 0
  expect_out "whole number"
done

echo "$passed checks passed, $failed failed"
[ "$failed" = 0 ]
