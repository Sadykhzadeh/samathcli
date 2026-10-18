#!/bin/sh
# Stands in for curl while test/e2e.sh runs: answers the way SaMathAPI does,
# and writes the URL of every request to $CURL_LOG.
#
# Question n reads "<n>+1 <four options> <n+1>", and its answer sits in
# option n mod 4, so the right letters for a run are A B C D A B C D ...
# Without a seed n counts the questions served so far; with seed=S, task i of
# a request is n = S + i, as the real API does.
#
# The URL steers it: "down" fails to connect, "garbage" answers with a page
# that is not a question, "broken" answers every request with a 500,
# "timeout" runs out of time, "refuse-count" answers 400 to a request with
# count, and "drops-out"
# answers the first request and fails to connect after that. FAKE_IGNORE_COUNT=1 makes it an older server that does not
# know count and sends one question whatever is asked. "few" knows only 150
# tasks, n mod 150, and starts a request without a seed at half the number
# served so far, so the second request of a run repeats half of the first.

url=
while [ $# -gt 0 ]; do
  if [ "$1" = "--url" ]; then
    url=$2
    shift
  fi
  shift
done
echo "$url" >> "$CURL_LOG"
# Descriptors, other than standard input, that are open on /dev/null: curl is
# not meant to inherit any.
ls -l /dev/fd/ | awk '$NF == "/dev/null" && $(NF - 2) != 0' > "$CURL_LOG.fds"

case "$url" in
  *down*)
    echo "curl: (7) Failed to connect" >&2
    exit 7
    ;;
  *garbage*)
    echo "<html><body>502 Bad Gateway</body></html>"
    exit 0
    ;;
  *nul*)
    printf '1+1 2,3,4,5 2\n2+2 4,5,6,7 4\000\n3+3 9,8,7,6 6\n'
    exit 0
    ;;
  *timeout*)
    echo "curl: (28) Operation timed out after 10001 milliseconds" >&2
    exit 28
    ;;
  *broken*)
    echo "curl: (22) The requested URL returned error: 500" >&2
    exit 22
    ;;
  *refuse-count*count=*)
    echo "curl: (22) The requested URL returned error: 400" >&2
    exit 22
    ;;
esac

count=1
case "$url" in
  *count=*)
    count=${url#*count=}
    count=${count%%&*}
    ;;
esac
if [ -n "${FAKE_IGNORE_COUNT:-}" ]; then count=1; fi

seed=
case "$url" in
  *seed=*)
    seed=${url#*seed=}
    seed=${seed%%&*}
    ;;
esac

served=0
if [ -f "$CURL_LOG.served" ]; then served=$(cat "$CURL_LOG.served"); fi

case "$url" in
  *drops-out*)
    if [ "$served" -gt 0 ]; then
      echo "curl: (7) Failed to connect" >&2
      exit 7
    fi
    ;;
esac

i=0
while [ "$i" -lt "$count" ]; do
  if [ -n "$seed" ]; then n=$((seed + i)); else n=$((served + i)); fi
  case "$url" in
    *few*)
      if [ -z "$seed" ]; then n=$((served / 2 + i)); fi
      n=$((n % 150))
      ;;
  esac
  answer=$((n + 1))
  place=$((n % 4))
  options=
  k=0
  while [ "$k" -lt 4 ]; do
    value=$((answer + (k - place + 4) % 4))
    if [ -z "$options" ]; then options=$value; else options=$options,$value; fi
    k=$((k + 1))
  done
  echo "$n+1 $options $answer"
  i=$((i + 1))
done
echo $((served + count)) > "$CURL_LOG.served"
