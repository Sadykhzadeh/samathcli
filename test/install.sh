#!/bin/sh
# Checks that make install puts samath and its manual page where make
# uninstall finds them, and that uninstall takes both away again. Run from
# `make check`, with the binary already built.

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
bin="$work/usr/local/bin/samath"
man="$work/usr/local/share/man/man1/samath.1"
completion="$work/usr/local/share/bash-completion/completions/samath"

make -s -C "$root" install DESTDIR="$work" PREFIX=/usr/local MANDIR=/usr/local/share/man COMPLETIONDIR=/usr/local/share/bash-completion/completions || exit 1
if [ ! -x "$bin" ] || [ ! -f "$man" ] || [ ! -f "$completion" ]; then
  echo "make install did not put samath, samath.1 and the completion in place" >&2
  exit 1
fi

make -s -C "$root" uninstall DESTDIR="$work" PREFIX=/usr/local MANDIR=/usr/local/share/man COMPLETIONDIR=/usr/local/share/bash-completion/completions || exit 1
if [ -e "$bin" ] || [ -e "$man" ] || [ -e "$completion" ]; then
  echo "make uninstall left samath, samath.1 or the completion behind" >&2
  exit 1
fi

# A prefix with a space in it, as a home directory can have, is installed to
# and taken away from in the same way.
spaced="$work/with space"
make -s -C "$root" install DESTDIR="$work" PREFIX="/with space" || exit 1
if [ ! -x "$spaced/bin/samath" ] || [ ! -f "$spaced/share/man/man1/samath.1" ] || [ ! -f "$spaced/share/bash-completion/completions/samath" ]; then
  echo "make install did not put the files in a prefix with a space in it" >&2
  exit 1
fi
make -s -C "$root" uninstall DESTDIR="$work" PREFIX="/with space" || exit 1
if [ -e "$spaced/bin/samath" ] || [ -e "$spaced/share/man/man1/samath.1" ] || [ -e "$spaced/share/bash-completion/completions/samath" ]; then
  echo "make uninstall left files in a prefix with a space in it" >&2
  exit 1
fi
echo "install: make install and make uninstall put back and take away all three files"
