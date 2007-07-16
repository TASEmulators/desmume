#!/bin/sh
# There's an annoying bug in /usr/share/intltool/Makefile.in.in requiring
# me to modify it after a call to intltoolize. Since i can't seem to figure
# what's wrong, i'm commenting the call to intltoolize and adding the
# corresponding files to CVS.

#echo "Running intltoolize"
#intltoolize --copy --force --automake

autoreconf --install --force --verbose
