#!/bin/bash
source=$1
prefix=$2
pushd $source/desmume
sed -i 's/\r//' AUTHORS
for txtfile in AUTHORS
do
    iconv --from=ISO-8859-1 --to=UTF-8 $txtfile > tmp
    touch -r $txtfile tmp
    mv tmp $txtfile
done
find src -name *.[ch]* -exec chmod 644 {} \;
popd
pushd $source/desmume/src/frontend/posix
./autogen.sh
./configure --prefix=$prefix --bindir=$prefix/games --datadir=$prefix/share/games \
			--enable-gdb-stub \
			--enable-osmesa \
			--enable-glade
popd
