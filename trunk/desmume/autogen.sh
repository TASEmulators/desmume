#!/bin/sh
# intltoolize is optionnal as it's only required for the gtk-glade UI.

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
