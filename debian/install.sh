#!/bin/bash
source=$1
pushd desmume/src/frontend/posix
DESTDIR=$source/debian/desmume make install
popd
mv $source/debian/desmume/usr/games/share/pixmaps/DeSmuME.xpm \
$source/debian/desmume/usr/share/pixmaps/
rm -rf $source/debian/desmume/usr/share/games/pixmaps
mv $source/debian/desmume/usr/games/share/applications/desmume*.desktop \
$source/debian/desmume/usr/share/applications/
rm -rf $source/debian/desmume/usr/games/share/applications
