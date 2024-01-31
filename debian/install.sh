#!/bin/bash
source=$1
prefix=$2
destdir=$3
pushd $source/desmume/src/frontend/posix
DESTDIR=$destdir make install
popd
mv $destdir/$prefix/games/share/pixmaps/DeSmuME.xpm \
$destdir/$prefix/share/pixmaps/
rm -rf $destdir/$prefix/share/games/pixmaps
mv $destdir/$prefix/games/share/applications/desmume*.desktop \
$destdir/$prefix/share/applications/
rm -rf $destdir/$prefix/games/share/applications
