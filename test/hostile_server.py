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
        else:
            self.reply(404, b"not found\n")


if __name__ == "__main__":
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    print(server.server_address[1], flush=True)
    server.daemon_threads = True
    server.serve_forever()
    sys.exit(0)
