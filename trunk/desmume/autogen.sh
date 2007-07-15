#!/bin/sh
echo "Running intltoolize"
intltoolize --copy --force --automake

autoreconf --install --force --verbose
