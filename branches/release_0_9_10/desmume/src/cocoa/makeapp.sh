#!/bin/bash
#This script builds the application bundle so that DeSmuME can run when compiled from Code::Blocks

#
lipo build/codeblocks/DeSmuME.app/Contents/MacOS/DeSmuME_x86 DeSmuME.app/Contents/MacOS/DeSmuME_ppc -create -output DeSmuME.app/Contents/MacOS/DeSmuME
rm -f build/codeblocks/DeSmuME.app/Contents/MacOS/DeSmuME_x86
rm -f build/codeblocks/DeSmuME.app/Contents/MacOS/DeSmuME_ppc

#
mkdir -p build/codeblocks/DeSmuME.app/Contents/Resources
cp Info.plist build/codeblocks/DeSmuME.app/Contents/Info.plist
cp PkgInfo build/codeblocks/DeSmuME.app/Contents/PkgInfo
cp InfoPlist.strings build/codeblocks/DeSmuME.app/Contents/Resources/InfoPlist.strings
cp DeSmuME.icns build/codeblocks/DeSmuME.app/Contents/Resources/DeSmuME.icns
cp ../../COPYING build/codeblocks/DeSmuME.app/Contents/Resources/COPYING
cp ../../README build/codeblocks/DeSmuME.app/Contents/Resources/README
cp ../../README.MAC build/codeblocks/DeSmuME.app/Contents/Resources/README.MAC
cp ../../AUTHORS build/codeblocks/DeSmuME.app/Contents/Resources/AUTHORS
cp ../../README.TRANSLATION build/codeblocks/DeSmuME.app/Contents/Resources/README.TRANSLATION
cp ../../ChangeLog build/codeblocks/DeSmuME.app/Contents/Resources/ChangeLog

#Localizations
cp -R translations/*.lproj build/codeblocks/DeSmuME.app/Contents/Resources/
