#!/bin/bash
#This script builds the application bundle so that DeSmuME can run when compiled from Code::Blocks

#
lipo DeSmuME.app/Contents/MacOS/DeSmuME_x86 DeSmuME.app/Contents/MacOS/DeSmuME_ppc -create -output DeSmuME.app/Contents/MacOS/DeSmuME
rm -f DeSmuME.app/Contents/MacOS/DeSmuME_x86
rm -f DeSmuME.app/Contents/MacOS/DeSmuME_ppc

#
mkdir -p DeSmuME.app/Contents/Resources
cp Info.plist DeSmuME.app/Contents/Info.plist
cp PkgInfo DeSmuME.app/Contents/PkgInfo
cp InfoPlist.strings DeSmuME.app/Contents/Resources/InfoPlist.strings
cp DeSmuME.icns DeSmuME.app/Contents/Resources/DeSmuME.icns
cp ../../COPYING DeSmuME.app/Contents/Resources/COPYING
cp ../../README DeSmuME.app/Contents/Resources/README
cp ../../README.MAC DeSmuME.app/Contents/Resources/README.MAC
cp ../../AUTHORS DeSmuME.app/Contents/Resources/AUTHORS
cp ../../README.TRANSLATION DeSmuME.app/Contents/Resources/README.TRANSLATION
cp ../../ChangeLog DeSmuME.app/Contents/Resources/ChangeLog

#Localizations
cp -R translations/*.lproj DeSmuME.app/Contents/Resources/
