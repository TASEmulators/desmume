7z.dll-backed archive access for the Windows frontend.
=======================================================

The loader and COM stream code in this directory were adapted from
vba-rr-modernized (https://github.com/CarterLi/vba-rr-modernized,
GPL-2.0), which in turn derived the archive interface from the old
VBA/DeSmuME File_Extractor frontend.

No archive decoder is compiled into DeSmuME. Format handlers are loaded at
runtime from the official 7-Zip DLL shipped next to the executable:

  desmume/3rdparty/libs/x64/7z.dll     (x64)
  desmume/3rdparty/libs/arm64/7z.dll   (ARM64)

The shipped DLLs are unmodified 7-Zip 26.04 binaries from the official
installers. The interface headers under 7z/ must stay in sync with that
version; 7zapi.cpp logs a mismatch when GetModuleProp disagrees.

7-Zip is Copyright (c) 1999-2026 Igor Pavlov. Most of its code is LGPL;
the RAR decoder remains under the unRAR license restriction. Keep the
unRAR notice when redistributing a build that includes 7z.dll.
