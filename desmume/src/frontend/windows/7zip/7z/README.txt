This directory keeps only the 7-Zip headers that define the interfaces the
emulator implements (archive and extract callbacks, in/out streams) plus their
include dependencies.

The archive handlers are no longer compiled from source.  They are loaded from
the official 7z.dll at runtime; see ../7zapi.cpp for the search order and the
exported entry points.

The headers were taken from 7-Zip 26.04 and must stay in sync with whatever
7z.dll is shipped, because the interface layout is shared between the two.

The official DLLs live in win32/lib/x86/7z.dll and win32/lib/x64/7z.dll
(7-Zip 26.04, taken unmodified from the official installers).  The build copies
the one matching the target platform into the output directory next to the
emulator, the same way lua51.dll is deployed.  To upgrade, replace both files
and keep these headers in sync with the new version.
