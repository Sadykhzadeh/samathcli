"""A server that answers the way no SaMathAPI does, for test/hostile.sh.

usage: python3 test/hostile_server.py
It prints the port it listens on, once, and serves until it is killed.
"""
import http.server
import sys
import time

GARBAGE = b"this is not a question at all\n"
QUESTION = b"1+1 1,2,3,4 2\n"


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, body, delay=0.0):
        time.sleep(delay)
        self.send_response(status)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/garbage":
            self.reply(200, GARBAGE)
        elif path == "/huge":
            self.reply(200, QUESTION * (2 * 1024 * 1024 // len(QUESTION)))
        elif path == "/slow":
            self.reply(200, QUESTION, delay=20)
        elif path == "/broken":
            self.reply(500, b"internal error\n")
        elif path == "/nonutf8":
            self.reply(200, b"\xff\xfe\xfd 1,2,3,4 2\n")
        elif path == "/refuse":
            self.reply(400, b"\x1b[31mred\x1b[0m " + b"x" * 5000)
        elif path == "/empty":
            self.reply(200, b"")
        elif path == "/refused-bytes":
            self.reply(400, b"\x9b31mBAD \xe9 ok\n")
        elif path == "/refused-c1":
            self.reply(400, b"\xc2\x9b31mBAD\n")
        elif path == "/refused-bidi":
            self.reply(400, b"\xe2\x80\xaeBAD\xe2\x81\xa9 \xe2\x80\x8eok\xe2\x80\x8f\n")
        elif path == "/refused-utf8":
            self.reply(400, b"\xf0\x9f\x98\x94 caf\xc3\xa9 \xc0\xaf \xed\xa0\x80 \xf4\x90\x80\x80 \xf0\x8f\xbf\xbf \xe0\x80\xaf \xe2\x80 end\n")
        else:
            self.reply(404, b"not found\n")


if __name__ == "__main__":
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    print(server.server_address[1], flush=True)
    server.daemon_threads = True
    server.serve_forever()
    sys.exit(0)
