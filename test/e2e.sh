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

# How long standard input stays open after the answers; a question left alone
# can only run out of time while someone might still type.
hold=0
# What samath finds on its PATH, when that is not the same as the rest of this
# script; empty for all of it.
samath_path=
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
  (cd "$work/cwd" && { printf '%s' "$input"; sleep "$hold"; } | env PATH="${samath_path:-$PATH}" "$root/samath" "$@" > "$work/out" 2> "$work/err")
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

# The right letters for the next $1 questions, which are numbered from $2 (0
# unless there is a seed): A B C D A B C D ...
right() {
  i=${2:-0}
  end=$((i + $1))
  while [ "$i" -lt "$end" ]; do
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
if [ "$(grep -c 'count=100$' "$CURL_LOG")" = 3 ]; then ok; else fail "$(cat "$CURL_LOG")"; fi
expect_out "Fantastic"

# questions N : how many questions the run asked, and how many of them differ.
questions() {
  asked=$(printf '%s\n' "$out" | grep -c 'Solve this expression')
  different=$(printf '%s\n' "$out" | grep 'Solve this expression' | sed 's/.*: //' | sort -u | wc -l | tr -d ' ')
}

input=$(right 250 | tr -d '\n')
run_at "http://api.test/few?c=true" "a later batch does not ask a question again" 150
questions
if [ "$asked" = 150 ] && [ "$different" = 150 ]; then ok; else fail "$different different of $asked"; fi
expect_out "$(printf '101/150\t🤔 Solve this expression: 100+1')"
expect_requests 2

input=$(right 250 | tr -d '\n')
run_at "http://api.test/few?c=true" "one question left is asked for with a full batch" 101
expect_out "$(printf '101/101\t🤔 Solve this expression: 100+1')"
expect_url "few?c=true&count=100"
if [ "$(grep -c 'count=100$' "$CURL_LOG")" = 2 ]; then ok; else fail "$(cat "$CURL_LOG")"; fi

input=$(right 250 | tr -d '\n')
run_at "http://api.test/few?c=true" "a server out of new questions repeats them" 250
expect_status 0
questions
if [ "$asked" = 250 ] && [ "$different" = 150 ]; then ok; else fail "$different different of $asked"; fi
expect_out "$(printf '150/250\t🤔 Solve this expression: 149+1')"
expect_requests 3

input=$(right 200 | tr -d '\n')
run_at "http://api.test/few?c=true" "a run with a seed keeps the questions of its seed" -s 0 200
questions
if [ "$asked" = 200 ] && [ "$different" = 150 ]; then ok; else fail "$different different of $asked"; fi
expect_out "$(printf '151/200\t🤔 Solve this expression: 0+1')"
expect_url "count=100&seed=100"
expect_requests 2

input=$(printf 'b\nb\nb\nb\n')
run "wrong answers are listed with the right ones" 4
expect_status 0
expect_out "You have 3 mistakes!"
expect_out "Task 1. 0+1	= 1	(you said 2)"
expect_out "Task 3. 2+1	= 3	(you said 6)"
expect_out "Task 4. 3+1	= 4	(you said 6)"
expect_no_out "Task 2."
expect_out "1 of 4 right (25%)"
expect_out "1/4	🤔 Solve this expression: 0+1"
expect_out "4/4	🤔 Solve this expression: 3+1"

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
run_at "http://api.test/garbage?c=true" "a reply that is not a question says so" 3
expect_status 1
expect_err "answered, but not with questions"
expect_err "gen?c=true"
case "$err" in *"could not reach"*) fail "said the server was not there" ;; *) ok ;; esac

input=$(right 3)
samath_path=$work/cwd
run "a missing curl is said to be missing, not the server" 3
samath_path=
expect_status 1
expect_requests 0
expect_err "need curl"
case "$err" in *"could not reach"*) fail "blamed the server" ;; *) ok ;; esac

input=$(right 3)
run_at "http://api.test/broken?c=true" "a server that answers with an error says so" 3
expect_status 1
expect_requests 2
expect_err "answered with an error"
case "$err" in *"could not reach"*) fail "said the server was not there" ;; *) ok ;; esac

input=$(right 100)
run_at "http://api.test/drops-out?c=true" "a server that goes down mid-run" 150
expect_status 1
expect_requests 2
expect_out "SaMathAPI stopped answering after 100 of 150 questions."
expect_out "100 of 100 right (100%)"
expect_no_out "You stopped"
expect_err "could not reach SaMathAPI"

input=$(printf 'b')
FAKE_IGNORE_COUNT=1
export FAKE_IGNORE_COUNT
run_at "http://api.test/drops-out?c=true" "the questions answered so far are kept" -p 4
unset FAKE_IGNORE_COUNT
expect_status 1
expect_out "SaMathAPI stopped answering after 1 of 4 questions."
expect_out "Task 1. 0+1"
expect_out "0 of 1 right (0%)"
expect_no_out "Practice"
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

# The Linux console sends F1 to F4 as ESC [ [ A to ESC [ [ D.
input=$(printf 'a\033[[A\033[[Bb\033[[C\033[[D\033[[Ec')
run "function keys on the Linux console are not answers" 3
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

run "one question stopped at once is one question" 1
expect_status 0
expect_out "You stopped after 0 of 1 question."

input=$(right 5)
run "nothing is left behind in the directory" 5
if [ -z "$(ls -A "$work/cwd")" ]; then ok; else fail "left $(ls -A "$work/cwd")"; fi

input=$(right 1)
run_at "http://api.test/gen?c=true&x='\$(touch pwned)';touch pwned2" "the URL is never read by a shell" 1
if [ ! -e "$work/cwd/pwned" ] && [ ! -e "$work/cwd/pwned2" ]; then ok; else fail "a command in the URL ran"; fi
expect_url "\$(touch pwned)"

input=$(right 3)
run "a time limit that is kept changes nothing" -t 5 3
expect_status 0
expect_out "Fantastic"
run "the limit may come after the number" 3 --time=5
expect_status 0
expect_out "Fantastic"

# The input stays open and says nothing, so the second question runs out.
hold=2
input=$(printf 'a')
run "a question left alone runs out of time" -t 1 2
hold=0
expect_status 0
expect_out "You have 1 mistake!"
expect_out "Task 2. 1+1	= 2	(time ran out)"
expect_no_out "you said"
expect_no_out "Task 1."
expect_out "1 of 2 right (50%)"

# Questions 1 and 2 are answered at once and the third runs out its two
# seconds, so it is the slowest.
hold=3
input=$(printf 'ab')
run "the slowest question is named" -t 2 3
hold=0
expect_status 0
expect_out "2 of 3 right (67%)"
expect_out "Slowest: Task 3. 2+1 (2."

input=$(right 3)
run "a run that went by quickly names none" 3
expect_out "Fantastic"
expect_no_out "Slowest"

# With a seed, question n of the stand-in is seed + n - 1.
input=$(right 3 42)
run "a seed is passed on with the count" -s 42 3
expect_status 0
expect_requests 1
expect_url "http://api.test/gen?c=true&count=3&seed=42"
expect_out "Fantastic"

input=$(right 1 7)
run "one question with a seed is the plain request and the seed" -s 7 1
expect_status 0
if [ "$(cat "$CURL_LOG")" = "http://api.test/gen?c=true&seed=7" ]; then ok; else fail "asked for $(cat "$CURL_LOG")"; fi
expect_out "Fantastic"

input=$(right 250 10)
run "each batch starts where the last one ended" --seed=10 250
expect_status 0
expect_requests 3
expect_url "count=100&seed=10"
expect_url "count=100&seed=110"
expect_url "count=50&seed=210"
expect_out "Fantastic"

input=$(right 150 4294967295)
run "the seed wraps at 32 bits, as the API's does" -s 4294967295 150
expect_status 0
expect_url "count=100&seed=4294967295"
expect_url "count=50&seed=99"
expect_out "Fantastic"

input=$(right 3 5)
FAKE_IGNORE_COUNT=1
export FAKE_IGNORE_COUNT
run "a server that ignores count still gets each question's seed" -s 5 3
unset FAKE_IGNORE_COUNT
expect_status 0
expect_requests 3
expect_url "count=3&seed=5"
expect_url "c=true&seed=6"
expect_url "c=true&seed=7"
expect_out "Fantastic"

input=$(printf 'b')
run "the same seed is the same questions" -s 100 3
first=$(printf '%s\n' "$out" | grep '^Task')
run "the same seed, again" -s 100 3
if [ "$(printf '%s\n' "$out" | grep '^Task')" = "$first" ]; then ok; else fail "two runs differed"; fi
run "another seed" -s 101 3
if [ "$(printf '%s\n' "$out" | grep '^Task')" != "$first" ]; then ok; else fail "two seeds gave one run"; fi

for bad in "-s -1 5" "-s 4294967296 5" "-s many 5" "-s"; do
  input=
  # shellcheck disable=SC2086
  run "\"$bad\" is not a seed" $bad
  expect_status 1
  expect_requests 0
  expect_err "seed"
done

input=$(right 3)
run "a length and brackets are passed on with the count" -l 6 -b 3
expect_status 0
expect_requests 1
expect_url "http://api.test/gen?c=true&count=3&length=6&brackets=true"
expect_out "Fantastic"

input=$(right 1)
run "one question asks for its length alone" --length=4 1
expect_status 0
if [ "$(cat "$CURL_LOG")" = "http://api.test/gen?c=true&length=4" ]; then ok; else fail "asked for $(cat "$CURL_LOG")"; fi

input=$(right 2 9)
run "a length goes with a seed" -s 9 -l 3 2
expect_url "http://api.test/gen?c=true&count=2&seed=9&length=3"

input=$(right 3)
FAKE_IGNORE_COUNT=1
export FAKE_IGNORE_COUNT
run "a server that ignores count gets the length every time" -l 5 --brackets 3
unset FAKE_IGNORE_COUNT
expect_status 0
expect_requests 3
if [ "$(grep -c 'length=5&brackets=true' "$CURL_LOG")" = 3 ]; then ok; else fail "$(cat "$CURL_LOG")"; fi

input=$(right 3)
run "no negatives is passed on with the count" -n 3
expect_status 0
expect_requests 1
expect_url "http://api.test/gen?c=true&count=3&negatives=false"
expect_out "Fantastic"

input=$(right 1)
run "one question asks for no negatives alone" --no-negatives 1
if [ "$(cat "$CURL_LOG")" = "http://api.test/gen?c=true&negatives=false" ]; then ok; else fail "asked for $(cat "$CURL_LOG")"; fi

input=$(right 2 4294967295)
run "no negatives goes last, after everything else" -n -b -l 12 -s 4294967295 2
expect_url "http://api.test/gen?c=true&count=2&seed=4294967295&length=12&brackets=true&negatives=false"

input=$(right 3)
FAKE_IGNORE_COUNT=1
export FAKE_IGNORE_COUNT
run "a server that ignores count is asked for no negatives every time" -n 3
unset FAKE_IGNORE_COUNT
expect_requests 3
if [ "$(grep -c 'negatives=false' "$CURL_LOG")" = 3 ]; then ok; else fail "$(cat "$CURL_LOG")"; fi

input=
run "help mentions no negatives" --help
expect_out "--no-negatives"

for bad in "-l 1 5" "-l 13 5" "-l hard 5" "-l"; do
  input=
  # shellcheck disable=SC2086
  run "\"$bad\" is not a length" $bad
  expect_status 1
  expect_requests 0
  expect_err "2 to 12 numbers"
done

# Questions 1 and 3 are answered wrong, so they come back: the first is wrong
# again, then right, and the third is right at once.
input=$(printf 'b\nb\na\nb\nc\na\n')
run "practice asks the mistakes again until each is right" -p 3
expect_status 0
expect_requests 1
expect_out "You have 2 mistakes!"
expect_out "1 of 3 right (33%)"
expect_out "Practice, round 1: 2 to go"
expect_out "Practice, round 2: 1 to go"
expect_out "$(printf 'round 1: 2 to go\n\n1\t🤔 Solve this expression: 0+1')"
expect_out "Practice: every mistake put right, in 2 rounds."

input=$(printf 'b\nb\na\n')
run "a mistake put right at once is one round" -p 2
expect_out "in 1 round."
expect_no_out "rounds"

input=$(printf 'b\na\nq')
run "q stops the practice with what is left" -p 2
expect_status 0
expect_out "Practice stopped with 2 of 2 still to put right."
expect_no_out "put right, in"
expect_out "0 of 2 right (0%)"

input=$(printf 'bq')
run "a run that was stopped does not go on to practise" -p 3
expect_out "You stopped after 1 of 3 questions."
expect_no_out "Practice"

input=$(right 3)
run "nothing to practise is not mentioned" -p 3
expect_out "Fantastic"
expect_no_out "Practice"

input=$(printf 'b\nb\nb\nb\n')
run "without -p the mistakes are only listed" 4
expect_no_out "Practice"

# Question 1 is answered right, 2 wrong, and 3 and 4 are left to run out;
# each answer is spoken of above the next question.
hold=3
input=$(printf 'ac')
run "feedback says how each answer went" -f -t 1 4
hold=0
expect_status 0
expect_out "$(printf '✅ Right: 0+1 = 1\n\n2/4\t🤔 Solve this expression: 1+1')"
expect_out "$(printf '❌ Wrong: 1+1 = 2 (you said 3)\n\n3/4\t🤔 Solve this expression: 2+1')"
expect_out "$(printf '❌ Wrong: 2+1 = 3 (time ran out)\n\n4/4\t🤔 Solve this expression: 3+1')"
expect_out "You have 3 mistakes!"

input=$(printf 'b\na\n')
run "feedback goes on into the practice" --feedback -p 1
expect_out "$(printf '❌ Wrong: 0+1 = 1 (you said 2)\n\n🔁 Practice, round 1: 1 to go')"
expect_out "Practice: every mistake put right, in 1 round."

input=$(printf 'bb')
run "without -f nothing is said until the end" 2
expect_no_out "Wrong:"
expect_no_out "Right:"

input=
run "help says how to use it" --help
expect_status 0
expect_requests 0
expect_out "usage: samath"
expect_out "--time"

for bad in "-t 0 5" "-t 3601 5" "-t soon 5" "-t"; do
  input=
  # shellcheck disable=SC2086
  run "\"$bad\" is not a time limit" $bad
  expect_status 1
  expect_requests 0
  expect_err "time limit"
done

input=
run "an option that does not exist" -x 5
expect_status 1
expect_requests 0
expect_err "do not know the option \"-x\""

run "two numbers of questions" 5 6
expect_status 1
expect_requests 0
expect_err "\"6\" is a second one"

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
  expect_err "whole number"
done

echo "$passed checks passed, $failed failed"
[ "$failed" = 0 ]
