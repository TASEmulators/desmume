#!/bin/bash
#This script builds the application bundle so that DeSmuME can run.

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

#English
mkdir -p DeSmuME.app/Contents/Resources/English.lproj
cp -R English.nib DeSmuME.app/Contents/Resources/English.lproj/MainMenu.nib
cp English.strings DeSmuME.app/Contents/Resources/English.lproj/Localizable.strings

#Japanese
mkdir -p DeSmuME.app/Contents/Resources/Japanese.lproj
cp -R Japanese.nib DeSmuME.app/Contents/Resources/Japanese.lproj/MainMenu.nib
cp Japanese.strings DeSmuME.app/Contents/Resources/Japanese.lproj/Localizable.strings
