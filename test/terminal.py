"""Drives ./samath on a real pseudo-terminal, which a pipe cannot stand in for:
single keys, escape sequences, stopping, Ctrl-C, and what is left of the
terminal settings afterwards. Questions come from test/fakecurl.sh.

usage: python3 test/terminal.py ./samath
"""
import fcntl
import os
import pty
import re
import resource
import select
import shutil
import signal
import struct
import sys
import tempfile
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BINARY = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "samath"))

work = tempfile.mkdtemp()
os.mkdir(os.path.join(work, "bin"))
shutil.copy(os.path.join(HERE, "fakecurl.sh"), os.path.join(work, "bin", "curl"))
os.chmod(os.path.join(work, "bin", "curl"), 0o755)
os.environ["PATH"] = os.path.join(work, "bin") + os.pathsep + os.environ["PATH"]
os.environ["CURL_LOG"] = os.path.join(work, "requests")
os.environ["SAMATH_API_URL"] = "http://api.test/gen?c=true"
os.environ["TERM"] = "xterm"
open(os.environ["CURL_LOG"], "w").close()


def start(argv, closed_output=False, term="xterm", columns=0):
    # Every run starts with question 1, whose right answer is A.
    if os.path.exists(os.environ["CURL_LOG"] + ".served"):
        os.remove(os.environ["CURL_LOG"] + ".served")
    pid, fd = pty.fork()
    if pid == 0:
        # As a shell would start it: Python ignores SIGPIPE, and the program
        # would inherit that, and a signal the caller ignores is kept ignored
        # too. No core file from SIGQUIT either.
        for number in (signal.SIGPIPE, signal.SIGINT, signal.SIGQUIT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(number, signal.SIG_DFL)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        if closed_output:
            # Output into a pipe nobody reads, as in `samath 3 | true`.
            reader, writer = os.pipe()
            os.close(reader)
            os.dup2(writer, 1)
        os.environ["TERM"] = term
        if columns:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, columns, 0, 0))
        os.execv(BINARY, [BINARY] + argv)
    return pid, fd


def drain(fd, quiet=0.3):
    """Everything the program prints until it has been quiet for `quiet`."""
    out = b""
    end = time.time() + quiet
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            try:
                chunk = os.read(fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
            end = time.time() + quiet
    return out.decode("utf-8", "replace")


def modes(fd):
    lflag = termios.tcgetattr(fd)[3]
    return bool(lflag & termios.ECHO), bool(lflag & termios.ICANON)


def reap(pid):
    for _ in range(60):
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            return status
        time.sleep(0.05)
    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)
    return None


def questions(text):
    return len(re.findall(r"Solve this expression", text))


failed = 0


def check(label, ok, detail=""):
    global failed
    if not ok:
        failed += 1
        print("FAIL: %s %s" % (label, detail))


# One key is one answer; arrows and other keys are ignored; q stops.
pid, fd = start(["3"])
first = drain(fd)
check("the first question is shown", questions(first) == 1, first[-200:])
check("the wait for the questions is shown, then wiped",
      "Asking SaMathAPI" in first and first.index("Asking SaMathAPI") < first.index("\r\x1b[K") < first.index("Solve this"),
      repr(first[:200]))
echo, canon = modes(fd)
check("echo and line mode are off while asking", not echo and not canon, "echo=%s canon=%s" % (echo, canon))
os.write(fd, b"a")  # question 1 is answered by A
second = drain(fd)
check("one key, no Enter, moves on", questions(second) == 1 and "\x1b[H\x1b[2J" in second, second[-200:])
os.write(fd, b"\x1b[A")
arrow = drain(fd)
check("an arrow key changes nothing", questions(arrow) == 0 and "stopped" not in arrow and "mistake" not in arrow, arrow[-200:])
os.write(fd, b"\x1bOB")
check("an application-mode arrow changes nothing", questions(drain(fd)) == 0)
os.write(fd, b"\x1b[[A")
check("F1 on the Linux console changes nothing", questions(drain(fd)) == 0)
os.write(fd, b"\x1b[15~")
check("F5 changes nothing", questions(drain(fd)) == 0)
os.write(fd, b"\x1a")
check("Ctrl-Z is ignored", questions(drain(fd)) == 0)
os.write(fd, b"\x13")
check("Ctrl-S is ignored", questions(drain(fd)) == 0)
os.write(fd, b"7")
check("a digit is ignored", questions(drain(fd)) == 0)
os.write(fd, b"q")
summary = drain(fd)
check("q shows the result", "You stopped after 1 of 3 questions." in summary, summary[-300:])
status = reap(pid)
echo, canon = modes(fd)
check("the terminal is handed back", echo and canon, "echo=%s canon=%s" % (echo, canon))
check("Ctrl-S works again afterwards", termios.tcgetattr(fd)[0] & termios.IXON)
check("the exit status is 0", status is not None and os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0, str(status))
os.close(fd)

# Keys typed before the first question is drawn do not answer it.
pid, fd = start(["1"])
os.write(fd, b"b")
drained = drain(fd, 0.5)
check("a key typed ahead is not an answer", questions(drained) == 1 and "mistake" not in drained and "Fantastic" not in drained, drained[-200:])
os.write(fd, b"a")
check("and the real answer is", "Fantastic" in drain(fd))
reap(pid)
os.close(fd)

# A key pressed twice answers the question it was pressed for, not the next
# one as well, which would go by before it could be read.
pid, fd = start(["3"])
drain(fd)
os.write(fd, b"a")
time.sleep(0.08)
os.write(fd, b"a")
twice = drain(fd)
check("a double press answers one question", questions(twice) == 1 and "2/3" in twice and "3/3" not in twice, twice[-200:])
os.write(fd, b"b")  # question 2 is answered by B
check("and the next key answers the next", "3/3" in drain(fd))
os.write(fd, b"q")
check("both answers were right", "2 of 2 right" in drain(fd))
reap(pid)
os.close(fd)

# Ctrl-C, Ctrl-\, SIGTERM and output that nobody reads any more hand the
# terminal back and end the run by the signal.
for label, interrupt, sig, closed_output in (
    ("Ctrl-C", lambda pid, fd: os.write(fd, b"\x03"), signal.SIGINT, False),
    ("Ctrl-\\", lambda pid, fd: os.write(fd, b"\x1c"), signal.SIGQUIT, False),
    ("SIGTERM", lambda pid, fd: os.kill(pid, signal.SIGTERM), signal.SIGTERM, False),
    ("A closed output pipe", lambda pid, fd: None, signal.SIGPIPE, True),
):
    pid, fd = start(["3"], closed_output)
    drain(fd)
    interrupt(pid, fd)
    status = reap(pid)
    echo, canon = modes(fd)
    check(label + " hands the terminal back", echo and canon, "echo=%s canon=%s" % (echo, canon))
    check(label + " ends the run by the signal", status is not None and os.WIFSIGNALED(status) and os.WTERMSIG(status) == sig, str(status))
    os.close(fd)

# Ctrl-C part of the way through gives the result so far, as Q does, and then
# ends the run by SIGINT, so that a shell sees it was interrupted.
pid, fd = start(["3", "-t", "30"])
drain(fd)
os.write(fd, b"a")
drain(fd)
os.write(fd, b"\x03")
after = drain(fd)
status = reap(pid)
check("Ctrl-C gives the result so far", "You stopped after 1 of 3 question" in after and "1 of 1 right" in after, repr(after[-300:]))
check("Ctrl-C then ends the run by SIGINT", status is not None and os.WIFSIGNALED(status) and os.WTERMSIG(status) == signal.SIGINT, str(status))
echo, canon = modes(fd)
check("the terminal is handed back after Ctrl-C with a result", echo and canon, "echo=%s canon=%s" % (echo, canon))
os.close(fd)

# Ctrl-C while the questions are still being asked for ends the run as well,
# and does not blame the server.
os.environ["SAMATH_API_URL"] = "http://api.test/slow?c=true"
pid, fd = start(["3"])
time.sleep(0.5)
os.write(fd, b"\x03")
began = time.time()
after = drain(fd)
status = reap(pid)
os.environ["SAMATH_API_URL"] = "http://api.test/gen?c=true"
check("Ctrl-C while waiting gives the result", "You stopped after 0 of 3 questions." in after, repr(after[-300:]))
check("Ctrl-C while waiting does not blame the server", "could not reach" not in after and "SaMathAPI" not in after.replace("Asking SaMathAPI", ""), repr(after[-300:]))
check("Ctrl-C while waiting ends the run by SIGINT, at once", status is not None and os.WIFSIGNALED(status) and os.WTERMSIG(status) == signal.SIGINT and time.time() - began < 3, str(status))
os.close(fd)

# The same when the program alone gets the signal, as from `kill -INT`, while
# the server is still working on its answer.
os.environ["SAMATH_API_URL"] = "http://api.test/slow?c=true"
pid, fd = start(["3"])
time.sleep(0.5)
began = time.time()
os.kill(pid, signal.SIGINT)
after = drain(fd)
status = reap(pid)
os.environ["SAMATH_API_URL"] = "http://api.test/gen?c=true"
check("a SIGINT alone gives the result", "You stopped after 0 of 3 questions." in after, repr(after[-300:]))
check("a SIGINT alone ends the run by SIGINT, without waiting for the server", status is not None and os.WIFSIGNALED(status) and os.WTERMSIG(status) == signal.SIGINT and time.time() - began < 3, str(status))
os.close(fd)

def collect(fd, seconds):
    """Everything printed in the next `seconds`, quiet or not."""
    out = b""
    end = time.time() + seconds
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            try:
                out += os.read(fd, 65536)
            except OSError:
                break
    return out.decode("utf-8", "replace")


# A time limit counts down in place, and moves on when it runs out.
pid, fd = start(["-t", "2", "2"])
seen = collect(fd, 3.5)  # two seconds to run out, with room for a slow start
check("the limit is shown", "Answer (A-D, Q to stop) - \x1b72 s left: " in seen, seen[-300:])
check("it counts down in place", "\x1b81 s left: \x1b[K" in seen, seen[-300:])
# Going back to the start of the line instead would leave a trail of prompts
# in a terminal narrower than the line, which wraps it.
check("only the seconds are written again", seen.count("Answer (A-D") == questions(seen), seen[-300:])
check("a question that runs out moves on to the next", questions(seen) == 2, seen[-300:])
os.write(fd, b"a")  # the second question is answered by B, so this is a mistake
summary = collect(fd, 0.5)
check("and counts as a mistake", "You have 2 mistakes!" in summary and "(time ran out)" in summary, summary[-300:])
reap(pid)
os.close(fd)

# Answering in time stops the clock: nothing runs out afterwards. The key goes
# in only once the question is on the screen: a key typed while the questions
# are still being fetched is thrown away, so a slow fetch would fail this.
pid, fd = start(["-t", "1", "1"])
shown = ""
deadline = time.time() + 10
while "Solve this expression" not in shown and time.time() < deadline:
    shown += drain(fd, 0.05)
drain(fd, 0.4)
os.write(fd, b"a")
check("an answer in time is kept", "Fantastic" in collect(fd, 0.5))
reap(pid)
os.close(fd)

# A dumb terminal cannot clear the screen or go back to a saved position, so
# it gets no codes for either: a blank line between questions, and the limit
# written once.
pid, fd = start(["-t", "2", "2"], term="dumb")
seen = collect(fd, 0.6)
os.write(fd, b"a")
seen += collect(fd, 2.5)
check("a dumb terminal gets no escape codes", "\x1b" not in seen, repr(seen[-300:]))
check("and is still asked both questions", questions(seen) == 2, seen[-300:])
check("with the limit written once each", seen.count("2 s left: ") == 2 and "1 s left" not in seen, seen[-300:])
check("and still reads single keys", "(time ran out)" in seen and "Task 1." not in seen, seen[-300:])
reap(pid)
os.close(fd)

# A server that answers one question at a time is waited for after each answer,
# and the wait is shown and wiped like the first one.
os.environ["FAKE_IGNORE_COUNT"] = "1"
pid, fd = start(["2"])
drain(fd)
os.write(fd, b"a")
waited = drain(fd)
del os.environ["FAKE_IGNORE_COUNT"]
check("a question fetched after an answer is waited for, then the wait wiped", "Asking SaMathAPI" in waited and waited.index("Asking SaMathAPI") < waited.index("\r\x1b[K") < waited.index("2/2"), waited[-300:])
os.write(fd, b"a")
reap(pid)
os.close(fd)

# A printout at a terminal shows the wait for its questions, and wipes it.
pid, fd = start(["-P", "2"])
shown = drain(fd)
check("a printout at a terminal shows the wait, then wipes it", "Asking SaMathAPI" in shown and shown.index("Asking SaMathAPI") < shown.index("\r\x1b[K") < shown.index("1. "), shown[-200:])
reap(pid)
os.close(fd)

# A printout at a terminal narrower than the wait line would wrap it, and the
# wipe would leave its first row behind, so the wait is not shown there at all.
pid, fd = start(["-P", "2"], columns=30)
narrow = drain(fd)
check("a printout at a narrow terminal shows no wait", "Asking" not in narrow and "1. " in narrow, narrow[-200:])
reap(pid)
os.close(fd)
pid, fd = start(["-P", "2"], columns=40)
exact = drain(fd)
check("a printout at a terminal exactly as wide as the wait shows it", "Asking SaMathAPI" in exact, exact[-200:])
reap(pid)
os.close(fd)

# A run wipes the wait with the question that follows it, so a narrow terminal
# still shows the wait for a batch.
os.environ["FAKE_IGNORE_COUNT"] = "1"
pid, fd = start(["2"], columns=30)
drain(fd)
os.write(fd, b"a")
narrowRun = drain(fd)
del os.environ["FAKE_IGNORE_COUNT"]
check("a run at a narrow terminal still shows the wait for a question", "Asking SaMathAPI" in narrowRun, narrowRun[-200:])
os.write(fd, b"a")
reap(pid)
os.close(fd)

# The answer key of a printout wraps between answers at a narrow terminal, so
# that no answer is split over two rows, and each row is as wide as the terminal.
pid, fd = start(["-P", "12"], columns=30)
key = drain(fd)
rows = [row for row in key[key.index("Answers:"):].splitlines()[1:] if row]
entries = [entry for row in rows for entry in row.split("   ")]
check("an answer key at a narrow terminal keeps each answer whole", len(entries) == 12 and all(re.fullmatch(r"\d+ [A-D]", entry) for entry in entries) and all(len(row) <= 30 for row in rows), repr(rows))
reap(pid)
os.close(fd)

def start_as_job(argv):
    # A shell with job control runs the program as its job, as an interactive
    # shell does, so that a stop comes from the shell's process group.
    pid, fd = pty.fork()
    if pid == 0:
        for number in (signal.SIGPIPE, signal.SIGINT, signal.SIGQUIT, signal.SIGTERM, signal.SIGHUP, signal.SIGTSTP):
            signal.signal(number, signal.SIG_DFL)
        os.environ["TERM"] = "xterm"
        os.execv("/bin/sh", ["sh", "-i"])
    os.write(fd, ("set -m\n%s %s\n" % (BINARY, " ".join(argv))).encode())
    return pid, fd


def stop_job(fd, shell, seconds):
    """Stops the program the shell started, as a Ctrl-Z in another terminal
    would, and reports the state it is in once it is stopped."""
    with open("/proc/%d/task/%d/children" % (shell, shell)) as children:
        program = int(children.read().split()[0])
    os.kill(program, signal.SIGTSTP)
    end = time.time() + seconds
    while time.time() < end and open("/proc/%d/stat" % program).read().split()[2] != "T":
        time.sleep(0.05)
    return program


# A stopped run hands the keyboard back, so the shell echoes what is typed, and
# the time a question has is not used up while it is stopped: the countdown
# goes on from the seconds that were left.
shell, fd = start_as_job(["-t", "2", "2"])
drain(fd)
stop_job(fd, shell, 2)
echo, canon = modes(fd)
check("echo and line mode are on while the run is stopped", echo and canon, "echo=%s canon=%s" % (echo, canon))
time.sleep(3)
os.write(fd, b"fg\n")
resumed = collect(fd, 3)
waiting = resumed.split("2/2")[0]  # what is shown before the first question runs out
check("the countdown goes on after fg, and the question runs out", "1 s left" in waiting and "2/2" in resumed, repr(resumed[:300]))
os.write(fd, b"q")
os.write(fd, b"exit\n")
drain(fd)
reap(shell)
os.close(fd)

# Ctrl-D stops the run.
pid, fd = start(["3"])
drain(fd)
os.write(fd, b"\x04")
check("Ctrl-D stops the run", "You stopped after 0 of 3 questions." in drain(fd))
reap(pid)
os.close(fd)

shutil.rmtree(work, ignore_errors=True)
if failed:
    print("%d terminal checks failed" % failed)
    sys.exit(1)
print("terminal checks passed")
