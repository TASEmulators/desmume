#!/bin/sh
# There's an annoying bug in /usr/share/intltool/Makefile.in.in requiring
# me to modify it after a call to intltoolize. Since i can't seem to figure
# what's wrong, i'm commenting the call to intltoolize and adding the
# corresponding files to CVS.

if test ! "x$(which intltoolize)" = "x"; then
  echo "Running intltoolize"
  intltoolize --copy --force --automake
else
  if test ! "x$(which gintltoolize)" = "x"; then
    echo "Running gintltoolize"
    gintltoolize --copy --force --automake
  fi
fi

autoreconf --install --force --verbose
