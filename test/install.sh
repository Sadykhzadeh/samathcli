#!/bin/sh
# Checks that make install puts samath and its manual page where make
# uninstall finds them, and that uninstall takes both away again. Run from
# `make check`, with the binary already built.

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
bin="$work/usr/local/bin/samath"
man="$work/usr/local/share/man/man1/samath.1"

make -s -C "$root" install DESTDIR="$work" PREFIX=/usr/local MANDIR=/usr/local/share/man || exit 1
if [ ! -x "$bin" ] || [ ! -f "$man" ]; then
  echo "make install did not put samath and samath.1 in place" >&2
  exit 1
fi

make -s -C "$root" uninstall DESTDIR="$work" PREFIX=/usr/local MANDIR=/usr/local/share/man || exit 1
if [ -e "$bin" ] || [ -e "$man" ]; then
  echo "make uninstall left samath or samath.1 behind" >&2
  exit 1
fi
echo "install: make install and make uninstall put both files back and take them away"
