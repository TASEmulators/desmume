#ifndef _ZZIP__MSVC_H
#define _ZZIP__MSVC_H 1
 
/* zzip/_msvc.h. Generated automatically at end of configure. */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef ZZIP_HAVE_BYTESWAP_H */

/* Define to 1 if you have the <direct.h> header file. */
#ifndef ZZIP_HAVE_DIRECT_H 
#define ZZIP_HAVE_DIRECT_H  1 
#endif

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
/* #undef ZZIP_HAVE_DIRENT_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef ZZIP_HAVE_DLFCN_H */

/* Define to 1 if you have the <fnmatch.h> header file. */
/* #undef ZZIP_HAVE_FNMATCH_H */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef ZZIP_HAVE_INTTYPES_H */

/* Define to 1 if you have the <io.h> header file. */
#ifndef ZZIP_HAVE_IO_H 
#define ZZIP_HAVE_IO_H  1 
#endif

/* Define to 1 if you have the <memory.h> header file. */
#ifndef ZZIP_HAVE_MEMORY_H 
#define ZZIP_HAVE_MEMORY_H  1 
#endif

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef ZZIP_HAVE_NDIR_H */

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef ZZIP_HAVE_STDINT_H */

/* Define to 1 if you have the <stdlib.h> header file. */
#ifndef ZZIP_HAVE_STDLIB_H 
#define ZZIP_HAVE_STDLIB_H  1 
#endif

/* Define to 1 if you have the `strcasecmp' function. */
/* #undef ZZIP_HAVE_STRCASECMP */

/* Define to 1 if you have the <strings.h> header file. */
/* #undef ZZIP_HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#ifndef ZZIP_HAVE_STRING_H 
#define ZZIP_HAVE_STRING_H  1 
#endif

/* Define to 1 if you have the `strndup' function. */
/* #undef ZZIP_HAVE_STRNDUP */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef ZZIP_HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/int_types.h> header file. */
/* #undef ZZIP_HAVE_SYS_INT_TYPES_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef ZZIP_HAVE_SYS_MMAN_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef ZZIP_HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef ZZIP_HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#ifndef ZZIP_HAVE_SYS_STAT_H 
#define ZZIP_HAVE_SYS_STAT_H  1 
#endif

/* Define to 1 if you have the <sys/types.h> header file. */
#ifndef ZZIP_HAVE_SYS_TYPES_H 
#define ZZIP_HAVE_SYS_TYPES_H  1 
#endif

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef ZZIP_HAVE_UNISTD_H */

/* Define to 1 if you have the <winbase.h> header file. */
#ifndef ZZIP_HAVE_WINBASE_H 
#define ZZIP_HAVE_WINBASE_H  1  /* hmm, is that win32 ? */ 
#endif

/* Define to 1 if you have the <windows.h> header file. */
#ifndef ZZIP_HAVE_WINDOWS_H 
#define ZZIP_HAVE_WINDOWS_H  1  /* yes, this is windows */ 
#endif

/* Define to 1 if you have the <winnt.h> header file. */
#ifndef ZZIP_HAVE_WINNT_H 
#define ZZIP_HAVE_WINNT_H  1      /* is that always true? */ 
#endif

/* Define to 1 if you have the <zlib.h> header file. */
#ifndef ZZIP_HAVE_ZLIB_H 
#define ZZIP_HAVE_ZLIB_H  1      /* you do have it, right? */ 
#endif

/* whether the system defaults to 32bit off_t but can do 64bit when requested
   */
/* #undef ZZIP_LARGEFILE_SENSITIVE */

/* Name of package */
#ifndef ZZIP_PACKAGE 
#define ZZIP_PACKAGE  "zziplib-msvc"     /* yes, make it known */ 
#endif

/* Define to the address where bug reports for this package should be sent. */
/* #undef ZZIP_PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef ZZIP_PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef ZZIP_PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef ZZIP_PACKAGE_TARNAME */

/* Define to the version of this package. */
/* #undef ZZIP_PACKAGE_VERSION */

/* The number of bytes in type int */
#ifndef ZZIP_SIZEOF_INT 
#define ZZIP_SIZEOF_INT  4 
#endif

/* The number of bytes in type long */
#ifndef ZZIP_SIZEOF_LONG 
#define ZZIP_SIZEOF_LONG  4 
#endif

/* The number of bytes in type short */
#ifndef ZZIP_SIZEOF_SHORT 
#define ZZIP_SIZEOF_SHORT  2 
#endif

/* Define to 1 if you have the ANSI C header files. */
#ifndef ZZIP_STDC_HEADERS 
#define ZZIP_STDC_HEADERS  1 
#endif

/* Version number of package */
#ifndef ZZIP_VERSION 
#define ZZIP_VERSION  "0.13.x" 
#endif

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef ZZIP_WORDS_BIGENDIAN */

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef ZZIP__FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef ZZIP__LARGE_FILES */

/* Define to `long long' if <sys/types.h> does not define. */
/* #undef ZZIP___int64 */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef _zzip_const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#ifndef _zzip_inline 
#define _zzip_inline  __inline 
#endif
#endif

/* Define to `_zzip_off_t' if <sys/types.h> does not define. */
#ifndef _zzip_off64_t 
#define _zzip_off64_t  __int64 
#endif

/* Define to `long' if <sys/types.h> does not define. */
#ifndef _zzip_off_t 
#define _zzip_off_t  long 
#endif

/* Define to equivalent of C99 restrict keyword, or to nothing if this is not
   supported. Do not define if restrict is supported directly. */
#ifndef _zzip_restrict 
#define _zzip_restrict  
#endif

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef _zzip_size_t */

/* Define to `int' if <sys/types.h> does not define. */
#ifndef _zzip_ssize_t 
#define _zzip_ssize_t  int 
#endif
 
/* once: _ZZIP__MSVC_H */
#endif
