"""Runs ./samath with the real curl against a small HTTP server on this
machine, which the stand-in curl of test/e2e.sh cannot check: that the flags
curl is started with work, and what a failing server does to a run.

usage: python3 test/realcurl.py ./samath
"""
import http.server
import os
import shutil
import subprocess
import sys
import threading
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


class Server(http.server.BaseHTTPRequestHandler):
    # The (status, body) to answer the next requests with; 200 once they are
    # used up.
    failures = []
    paths = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        Server.paths.append(self.path)
        if self.path.startswith("/moved"):
            # A server that has moved: http:// sent on to https:// is the
            # usual case, here it is a new path on the same server.
            self.send_response(301)
            self.send_header("Location", self.path.replace("/moved", "/gen", 1))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        status, said = Server.failures.pop(0) if Server.failures else (200, b"")
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
        self.wfile.write(body)


server = http.server.HTTPServer(("127.0.0.1", 0), Server)
threading.Thread(target=server.serve_forever, daemon=True).start()
url = "http://127.0.0.1:%d/gen?c=true" % server.server_address[1]


def run(args, failures=(), stdin=b"aaaaaaaaaa"):
    Server.failures = [f if isinstance(f, tuple) else (f, b"") for f in failures]
    Server.paths = []
    env = dict(os.environ, SAMATH_API_URL=url)
    done = subprocess.run([BINARY] + args, input=stdin, capture_output=True, env=env, timeout=60)
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

# A refusal that will not change is not asked again.
status, out, err = run(["1"], failures=[404, 404])
check(status == 1 and "answered with an error" in err, "a 404 is said so: %r" % err)
check(len(Server.paths) == 1, "a 404 is asked once: %r" % Server.paths)

status, out, err = run(["1"], failures=[503, 503])
check(status == 1 and "answered with an error" in err, "a server that stays down: %r" % err)
check(len(Server.paths) == 2, "a 503 is asked twice and no more: %r" % Server.paths)

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

# A URL that has moved is followed, not mistaken for an answer that is not
# questions.
url_ok = url
url = url.replace("/gen", "/moved")
status, out, err = run(["3"])
url = url_ok
check(status == 0 and "3 of 3 right" in out, "a moved URL is followed: %r %r" % (out, err))
check(Server.paths == ["/moved?c=true&count=3", "/gen?c=true&count=3"], "the redirect is followed once: %r" % Server.paths)

# A reply too big to be questions is said not to be questions, not to be out of
# reach.
url_ok = url
url = url.replace("/gen", "/big")
status, out, err = run(["3"])
url = url_ok
check(status == 1 and "not with questions" in err, "a huge reply: %r" % err)
check("could not reach" not in err, "a huge reply is not a server out of reach: %r" % err)

print("%d checks passed, %d failed" % (passed, failed))
sys.exit(1 if failed else 0)
