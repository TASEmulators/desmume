This copy of the 7-zip source code has been modified as follows:
- Encoding support has been disabled, so only archive extraction is supported.
- Some archive formats have been removed, so only 7z, bzip2, gzip, lzh, lzma, rar, split, tar, and zip are supported.
- Support for using it as a static library has been added. (specifically, a file called InitializeStaticLib.h)
- Occasional minor fixes/customization not really worth describing in detail.
- The LGPL part of the 7-zip library source license has been upgraded to GPL 2.0 (as per condition 3 of the LGPL 2.1)

You can find most of the files in this library in the "7-Zip Source code" download at http://www.7-zip.org/ distributed under the LGPL 2.1. A small subset of them are in the LZMA SDK which is public domain and can be found at http://www.7-zip.org/sdk.html



7-Zip 4.64 Sources
------------------

7-Zip is a file archiver for Windows 95/98/ME/NT/2000/2003/XP/Vista. 

7-Zip Copyright (C) 1999-2009 Igor Pavlov.


License Info
------------

This copy of 7-Zip is free software distributed under the GNU GPL 2.0
(with an additional restriction that has always applied to the unRar code).
read License.txt for more infomation about license.

Notes about unRAR license:

Please check main restriction from unRar license:

   2. The unRAR sources may be used in any software to handle RAR
      archives without limitations free of charge, but cannot be used
      to re-create the RAR compression algorithm, which is proprietary.
      Distribution of modified unRAR sources in separate form or as a
      part of other software is permitted, provided that it is clearly
      stated in the documentation and source comments that the code may
      not be used to develop a RAR (WinRAR) compatible archiver.

In brief it means:
1) You can compile and use compiled files under GNU GPL rules, since 
   unRAR license almost has no restrictions for compiled files.
   You can link these compiled files to GPL programs.
2) You can fix bugs in source code and use compiled fixed version.
3) You can not use unRAR sources to re-create the RAR compression algorithm.

---
Igor Pavlov
http://www.7-zip.org
