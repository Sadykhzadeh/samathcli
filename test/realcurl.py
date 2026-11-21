"""Runs ./samath with the real curl against a small HTTP server on this
machine, which the stand-in curl of test/e2e.sh cannot check: that the flags
curl is started with work, and what a failing server does to a run.

usage: python3 test/realcurl.py ./samath
"""
import http.server
import io
import os
import re
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
BINARY = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "samath"))

if not shutil.which("curl"):
    print("curl not found: the checks against a real server were skipped")
    sys.exit(0)

passed = 0
failed = 0


def check(condition, what):
    global passed, failed
    if condition:
        passed += 1
    else:
        failed += 1
        print("FAIL: " + what)


PROXY_VARS = ("http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY",
              "all_proxy", "ALL_PROXY", "no_proxy", "NO_PROXY")


def child_env(**more):
    # The caller's proxy settings would send the checks to a proxy, or let
    # no_proxy bypass the local server; each check sets the ones it needs.
    env = {name: value for name, value in os.environ.items() if name not in PROXY_VARS}
    env.update(more)
    return env


def own_signals():
    # A signal the caller ignored is inherited by the program, which then keeps
    # it: samath only stops curl on a signal it can catch.
    for number in (signal.SIGINT, signal.SIGQUIT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(number, signal.SIG_DFL)


class Server(http.server.BaseHTTPRequestHandler):
    # The (status, body) to answer the next requests with; 200 once they are
    # used up. A status of 0 is a connection dropped with no answer at all.
    failures = []
    paths = []
    closed = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        Server.paths.append(self.path)
        if self.path.startswith("/loop"):
            self.send_response(302)
            self.send_header("Location", self.path)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path.startswith("/to-file"):
            self.send_response(302)
            self.send_header("Location", "file:///etc/passwd")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path.startswith("/moved"):
            # A server that has moved: http:// sent on to https:// is the
            # usual case, here it is a new path on the same server.
            self.send_response(301)
            self.send_header("Location", self.path.replace("/moved", "/gen", 1))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path.startswith("/stall-big"):
            # More than a megabyte, and then nothing more: a run that is told so
            # does not wait for the rest of the answer.
            body = b"x" * 1050000
            self.send_response(200)
            self.send_header("Content-Length", str(len(body) + 50))
            self.end_headers()
            self.wfile.write(body)
            self.wfile.flush()
            time.sleep(30)
            return
        if self.path.startswith("/stall"):
            # Never answers. The time it takes curl to go away is taken from
            # when it closes its connection, which it does when it is stopped.
            deadline = time.time() + 30
            while time.time() < deadline:
                ready, _, _ = select.select([self.connection], [], [], 0.1)
                if ready and not self.connection.recv(1, socket.MSG_PEEK):
                    Server.closed.append((self.path, time.time()))
                    break
            self.close_connection = True
            return
        if self.path.startswith("/reset"):
            # Part of an answer, then the connection is reset rather than closed.
            self.send_response(200)
            self.send_header("Content-Length", "500")
            self.end_headers()
            self.wfile.write(b"1+1 2,3,4,5 2\n")
            self.wfile.flush()
            time.sleep(0.2)
            self.connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            self.wfile.close()
            self.rfile.close()
            self.connection.close()
            self.wfile = io.BytesIO()
            self.close_connection = True
            return
        if self.path.startswith("/cut"):
            # A reply that stops short of its Content-Length, as a server does
            # when it fails part of the way through an answer.
            self.send_response(200)
            self.send_header("Content-Length", "500")
            self.end_headers()
            self.wfile.write(b"1+1 2,3,4,5 2\n")
            self.close_connection = True
            return
        status, said = Server.failures.pop(0) if Server.failures else (200, b"")
        if status == 0:
            self.close_connection = True
            return
        query = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        count = int(query.get("count", ["1"])[0])
        seed = int(query.get("seed", ["0"])[0])
        body = said
        if status == 200 and "big" in self.path:
            # More than any run of questions: a web page, say.
            body = b"<p>not a question</p>\n" * 100000
        elif status == 200:
            for n in range(seed, seed + count):
                body += ("%d+1 %d,%d,%d,%d %d\n" % (n, n + 1, n + 2, n + 3, n + 4, n + 1)).encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            # curl stops reading once it has what it needs, as with a reply past 1 MB.
            pass


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Server)
threading.Thread(target=server.serve_forever, daemon=True).start()
url = "http://127.0.0.1:%d/gen?c=true" % server.server_address[1]


def run(args, failures=(), stdin=b"aaaaaaaaaa", **more):
    Server.failures = [f if isinstance(f, tuple) else (f, b"") for f in failures]
    Server.paths = []
    env = child_env(SAMATH_API_URL=url, **more)
    done = subprocess.run([BINARY] + args, input=stdin, capture_output=True, env=env,
                          preexec_fn=own_signals, timeout=60)
    return done.returncode, done.stdout.decode(), done.stderr.decode()


status, out, err = run(["5"])
check(status == 0 and "5 of 5 right" in out, "a run against a real server: %r %r" % (out, err))
check(Server.paths == ["/gen?c=true&count=5"], "the whole run in one request: %r" % Server.paths)

status, out, err = run(["3", "-s", "40"])
check(status == 0 and "Fantastic" in out, "a seeded run: %r" % out)
check(Server.paths == ["/gen?c=true&count=3&seed=40"], "the seed is sent: %r" % Server.paths)

# A server that fails once and then answers: the run goes on, and the
# questions asked for are the same.
for failing in (500, 502, 503):
    status, out, err = run(["4"], failures=[failing])
    check(status == 0 and "4 of 4 right" in out, "%d once is got over: %r %r" % (failing, out, err))
    check(Server.paths == ["/gen?c=true&count=4"] * 2, "%d is asked again: %r" % (failing, Server.paths))

# What a failing server says does not end in a new line, as SaMathAPI's
# "could not generate a task" does not. It is no part of the answer that
# comes after it: a run with a seed asks its own questions, each once, and
# the server is asked nothing more.
said = b"could not generate a task"
status, out, err = run(["4", "-s", "40"], failures=[(500, said)])
asked = re.findall(r"(\d+)\+1", out)
check(status == 0 and asked == ["40", "41", "42", "43"], "the questions after a 500 that said something: %r %r" % (out, err))
check(Server.paths == ["/gen?c=true&count=4&seed=40"] * 2, "a 500 that said something is asked again and no more: %r" % Server.paths)

status, out, err = run(["1", "-s", "40"], failures=[(500, said)])
asked = re.findall(r"(\d+)\+1", out)
check(status == 0 and asked == ["40"], "one question after a 500 that said something: %r %r" % (out, err))
check(Server.paths == ["/gen?c=true&seed=40"] * 2, "one question is asked again and no more: %r" % Server.paths)

# A connection that is dropped is asked again too, once.
status, out, err = run(["4"], failures=[0])
check(status == 0 and "4 of 4 right" in out, "a dropped connection is got over: %r %r" % (out, err))
check(Server.paths == ["/gen?c=true&count=4"] * 2, "a dropped connection is asked again: %r" % Server.paths)

status, out, err = run(["4"], failures=[0, 0])
check(status == 1 and "dropped the connection" in err, "a server that keeps dropping the connection: %r" % err)
check(Server.paths == ["/gen?c=true&count=4"] * 2, "a dropped connection is asked twice: %r" % Server.paths)
Server.paths = []
done = subprocess.run([BINARY, "1"], input=b"a", capture_output=True, timeout=60,
                      env=child_env(SAMATH_API_URL=url.replace("/gen", "/reset")))
check(done.returncode == 1 and "dropped the connection" in done.stderr.decode(),
      "a connection reset after part of an answer is said so: %r" % done.stderr)
check("Is it running" not in done.stderr.decode(), "a reset is not a server that is down: %r" % done.stderr)
check("Is it running" not in err and "could not reach" not in err, "a dropped connection is not a server that is down: %r" % err)
check(len(Server.paths) == 2, "a dropped connection is asked twice and no more: %r" % Server.paths)

# A refusal that will not change is not asked again.
status, out, err = run(["1"], failures=[404, 404])
check(status == 1 and "answered with an error" in err, "a 404 is said so: %r" % err)
check(len(Server.paths) == 1, "a 404 is asked once: %r" % Server.paths)

status, out, err = run(["1"], failures=[503, 503])
check(status == 1 and "answered with an error" in err, "a server that stays down: %r" % err)
check(len(Server.paths) == 2, "a 503 is asked twice and no more: %r" % Server.paths)

status, out, err = run(["4"], failures=[503, 503])
check(status == 1 and "answered with an error" in err, "a server that stays down for a batch: %r" % err)
check(len(Server.paths) == 2, "a batch is asked twice and no more: %r" % Server.paths)

# What the server says is said, so that a request it cannot do is fixed from
# the message and not by guessing.
said = b'length must be a whole number from 2 to 12, not "40"'
status, out, err = run(["1"], failures=[(400, said)])
check(status == 1 and "answered with an error" in err, "a 400 is said so: %r" % err)
check(said.decode() in err, "what a 400 says is shown: %r" % err)
check(len(Server.paths) == 1, "a 400 is asked once: %r" % Server.paths)

status, out, err = run(["1"], failures=[(503, b"busy"), (503, b"busy")])
check("busy" in err, "what a 503 says is shown: %r" % err)

# A message with control characters in it, and a long one, are not let through
# as they are.
status, out, err = run(["1"], failures=[(400, b"\x1b[2Jbad\x07\n" + b"x" * 5000)])
check("\x1b" not in err and "\x07" not in err, "control characters are left out: %r" % err)
check(len(err) < 600, "a long message is cut: %d bytes" % len(err))

# The C1 controls, which UTF-8 spells as two bytes, are left out too; the
# copyright sign, which is not one, is kept.
status, out, err = run(["1"], failures=[(400, b"bad\xc2\x9b2J\xc2\x9d\xc2\xa9 ok")])
check("\u009b" not in err and "\u009d" not in err, "C1 controls are left out: %r" % err)
check("bad2J© ok" in err, "the text around them is kept: %r" % err)

# A URL that has moved is followed, not mistaken for an answer that is not
# questions.
url_ok = url
url = url.replace("/gen", "/moved")
status, out, err = run(["3"])
url = url_ok
check(status == 0 and "3 of 3 right" in out, "a moved URL is followed: %r %r" % (out, err))
check(Server.paths == ["/moved?c=true&count=3", "/gen?c=true&count=3"], "the redirect is followed once: %r" % Server.paths)

# A URL curl cannot fetch from is blamed on the URL, not on a server that is
# not running; a redirect that never ends is too, and is not asked again.
url_ok = url
url = url.replace("http://", "htp://", 1)
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "not an address I can fetch from" in err, "a mistyped scheme is said so: %r" % err)
url_ok = url
url = url + " x"
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "with no spaces in it" in err, "a URL with a space is said so: %r" % err)
check(Server.paths == [], "a URL with a space is not sent: %r" % Server.paths)
check("could not reach" not in err, "a mistyped scheme is not a server out of reach: %r" % err)
check(Server.paths == [], "a mistyped scheme is not asked for: %r" % Server.paths)

# A file, or any scheme other than http and https, is refused too: curl would
# read the file, and its lines that look like questions would be asked.
local_dir = tempfile.mkdtemp()
local_file = os.path.join(local_dir, "questions")
with open(local_file, "w") as questions:
    questions.write("1+1 2,3,4,5 2\n")
url_ok = url
url = "file://" + local_file
status, out, err = run(["1"])
url = url_ok
shutil.rmtree(local_dir)
check(status == 1 and "not an address I can fetch from" in err, "a file is not fetched from: %r" % err)
check("Solve this expression" not in out, "a file's questions are not asked: %r" % out)

# A URL too long to ask for is a wrong address, and nothing is sent.
url_ok = url
url = url + "&x=" + "a" * 2100
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "not an address I can fetch from" in err, "a URL too long to ask for is said so: %r" % err)
check(Server.paths == [], "a URL too long to ask for is not sent: %r" % Server.paths)

# A host that does not resolve is a wrong address, not a server that is down.
url_ok = url
url = "http://no-such-host.invalid/gen?c=true"
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "could not find the host" in err, "a host that does not resolve is said so: %r" % err)
check("Is it running" not in err, "a host that does not resolve is not a server that is down: %r" % err)

# A proxy from http_proxy that does not resolve is blamed on the proxy: the
# server is not asked, and it is not said to be down.
url_ok = url
status, out, err = run(["1"], http_proxy="http://no-such-proxy.invalid:3128", all_proxy="")
url = url_ok
check(status == 1 and "could not find the proxy" in err, "a proxy that does not resolve is said so: %r" % err)
check("Is it running" not in err, "a proxy that does not resolve is not a server that is down: %r" % err)
check(Server.paths == [], "a proxy that does not resolve is not sent to: %r" % Server.paths)

# A redirect to a file, or any other scheme that is not followed, is the
# server's address that is wrong, not the one in SAMATH_API_URL, which is fine.
url_ok = url
url = url.replace("/gen", "/to-file")
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "redirected to an address that is not http or https" in err, "a redirect to a file is said so: %r" % err)
check("not an address I can fetch from" not in err, "a redirect to a file is not blamed on SAMATH_API_URL: %r" % err)
check(Server.paths == ["/to-file?c=true"], "a redirect to a file is not followed: %r" % Server.paths)

# A reply that stops part of the way through is a server that is there and
# failing: it is asked again once, and then said to have stopped.
url_ok = url
url = url.replace("/gen", "/cut")
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "stopped part of the way" in err, "a cut-short reply is said so: %r" % err)
check("could not reach" not in err and "Is it running" not in err, "a cut-short reply is not a server out of reach: %r" % err)
check(len(Server.paths) == 2, "a cut-short reply is asked twice and no more: %r" % Server.paths)

# An https address that is not answered with TLS, as a plain HTTP server is
# not, is a secure connection that could not be made: not a server out of reach.
url_ok = url
url = url.replace("http://", "https://", 1)
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "secure connection" in err, "a TLS failure is said so: %r" % err)
check("Is it running" not in err, "a TLS failure is not a server that is down: %r" % err)

# A certificate store that curl cannot load, as CURL_CA_BUNDLE naming a file
# that is not there does, is a certificate problem and not a server that is
# down: curl fails before it ever connects.
Server.paths = []
done = subprocess.run([BINARY, "1"], input=b"a", capture_output=True, timeout=60,
                      env=child_env(SAMATH_API_URL=url.replace("http://", "https://", 1),
                                    CURL_CA_BUNDLE="/nonexistent"))
err = done.stderr.decode()
check(done.returncode == 1 and "secure connection" in err, "a CA bundle that cannot be loaded: %r" % err)
check("Is it running" not in err, "a bad CA bundle is not a server that is down: %r" % err)

url_ok = url
url = url.replace("/gen", "/loop")
status, out, err = run(["1"])
url = url_ok
check(status == 1 and "redirects more than 5 times" in err, "a redirect that never ends is said so: %r" % err)
check(len(Server.paths) == 6, "a redirect that never ends is followed five times: %r" % Server.paths)

# A reply too big to be questions is said not to be questions, not to be out of
# reach.
url_ok = url
url = url.replace("/gen", "/big")
status, out, err = run(["3"])
url = url_ok
check(status == 1 and "more than 1 MB" in err, "a huge reply: %r" % err)
check("could not reach" not in err, "a huge reply is not a server out of reach: %r" % err)
check("curl" not in err and "Failure" not in err, "a huge reply shows nothing of curl: %r" % err)
check(err.count("\n") == 1, "a huge reply is one line: %r" % err)

# A curlrc with `fail` in it is not read, so a refusal still comes with its
# body and is a refusal like any other.
home = tempfile.mkdtemp()
with open(os.path.join(home, ".curlrc"), "w") as rc:
    rc.write("fail\n")
status, out, err = run(["1"], failures=[404, 404], CURL_HOME=home)
check(status == 1 and "answered with an error" in err, "a refusal with fail in the curlrc: %d %r" % (status, err))
status, out, err = run(["3"], failures=[404], CURL_HOME=home)
check(status == 0 and "3 of 3 right" in out, "a count refused with fail in the curlrc: %d %r %r" % (status, out, err))
shutil.rmtree(home)

# Nor does a curlrc with `retry` in it make curl try again by itself.
home = tempfile.mkdtemp()
with open(os.path.join(home, ".curlrc"), "w") as rc:
    rc.write("retry = 3\n")
status, out, err = run(["4", "-s", "40"], failures=[(500, said), (500, said)], CURL_HOME=home)
check(status == 1 and len(Server.paths) == 2, "a 500 with retry in the curlrc is asked twice: %d %r" % (status, Server.paths))
shutil.rmtree(home)

# A curlrc that would change the request, or send the answer to a file, is not
# read: the questions are asked for as they always are.
home = tempfile.mkdtemp()
saved = os.path.join(home, "saved")
with open(os.path.join(home, ".curlrc"), "w") as rc:
    rc.write('request = "POST"\noutput = "%s"\n' % saved)
status, out, err = run(["4"], CURL_HOME=home)
check(status == 0 and "4 of 4 right" in out, "a curlrc that changes the request is not read: %d %r %r" % (status, out, err))
check(Server.paths == ["/gen?c=true&count=4"], "the request is a GET whatever the curlrc says: %r" % Server.paths)
check(not os.path.exists(saved), "the curlrc's output file is not written")
shutil.rmtree(home)

# With standard output closed the questions are still fetched: the pipe that
# curl writes to must not be given the descriptor that is closed. The result
# then cannot be written, and the run says so.
Server.failures = []
Server.paths = []
done = subprocess.run([BINARY, "2"], input=b"ab", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                      env=child_env(SAMATH_API_URL=url), preexec_fn=lambda: os.close(1), timeout=60)
err = done.stderr.decode()
check(Server.paths == ["/gen?c=true&count=2"], "questions are fetched with standard output closed: %r" % Server.paths)
check("could not reach" not in err, "closed standard output is not a server out of reach: %r" % err)
check(done.returncode == 1 and "could not write the result" in err, "a result that cannot be written is said so: %r" % err)

# A reply past 1 MB that then stops is said to be too big at once, not after
# curl has waited out its time limit for the rest of it.
url_ok = url
url = url.replace("/gen", "/stall-big")
started = time.time()
status, out, err = run(["3"])
waited = time.time() - started
url = url_ok
check(status == 1 and "more than 1 MB" in err, "a reply past 1 MB is said so: %r" % err)
check(waited < 5, "a reply past 1 MB is not waited for: %.1f s" % waited)

# Ctrl-C with the answers piped in gives the result so far, as it does at a
# terminal, and then ends the run by SIGINT.
read_end, write_end = os.pipe()
piped = subprocess.Popen([BINARY, "3"], stdin=read_end, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, env=child_env(SAMATH_API_URL=url),
                         preexec_fn=own_signals)
os.close(read_end)
os.write(write_end, b"a")
time.sleep(1)
piped.send_signal(signal.SIGINT)
piped_out, piped_err = piped.communicate(timeout=30)
os.close(write_end)
check(piped.returncode == -signal.SIGINT, "Ctrl-C with piped answers ends the run by SIGINT: %r" % piped.returncode)
check("You stopped after 1 of 3 questions." in piped_out.decode() and "1 of 1 right" in piped_out.decode(),
      "Ctrl-C with piped answers gives the result so far: %r" % piped_out.decode()[-300:])

# A run stopped by a signal that reaches only the program, as from `kill`, takes
# curl down with it: curl is not left to wait out its time limit of 10 s.
for name, number in (("SIGTERM", signal.SIGTERM), ("SIGINT", signal.SIGINT)):
    Server.paths = []
    stalled = child_env(SAMATH_API_URL=url.replace("/gen", "/stall-" + name))
    run_once = subprocess.Popen([BINARY, "3"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, env=stalled, preexec_fn=own_signals)
    deadline = time.time() + 10
    while not Server.paths and time.time() < deadline:
        time.sleep(0.05)
    sent = time.time()
    run_once.send_signal(number)
    run_once.communicate(timeout=30)
    deadline = time.time() + 9
    while not [1 for path, _ in Server.closed if path.startswith("/stall-" + name)] and time.time() < deadline:
        time.sleep(0.05)
    gone = [when - sent for path, when in Server.closed if path.startswith("/stall-" + name)]
    left = gone[0] if gone else None
    check(run_once.returncode == -number, "%s ends the run by the signal: %r" % (name, run_once.returncode))
    check(left is not None and left < 3, "%s stops curl with the run: %r s" % (name, left))

print("%d checks passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
