/*
 * unlike in <zzip/conf.h> we are allowed to make up typedefs here,
 * while over there only #defines are allowed
 *
 * Author: 
 *	Guido Draheim <guidod@gmx.de>
 *
 *	Copyright (c) 2003,2004 Guido Draheim
 * 	    All rights reserved, 
 *          usage allowed under the restrictions of the
 *	    Lesser GNU General Public License 
 *          or alternatively the restrictions 
 *          of the Mozilla Public License 1.1
 *
 * This file is usually the first to define some real symbols. If you do
 * see some errors here then it is most likely the includepath is wrong
 * or some includeheader is missing / unreadable on your system.
 * (a) we include local headers with a "zzip/" prefix just to be sure
 *     to not actually get the wrong one. Consider to add `-I..` somewhere
 *     and especially VC/IDE users (who make up their own workspace files)
 *     should include the root source directory of this project.
 * (b) size_t and ssize_t are sometimes found be `configure` but they are
 *     not in the usual places (ANSI C = stddef.h; UNIX = sys/types.h), so
 *     be sure to look for them and add the respective header as an #include.
 */

#ifndef _ZZIP_TYPES_H_
#define _ZZIP_TYPES_H_

#include <zzip/conf.h>
#include <fcntl.h>
#include <stddef.h> /* size_t and friends */
#ifdef ZZIP_HAVE_SYS_TYPES_H
#include <sys/types.h> /* bsd (mac) has size_t here */
#endif
/* msvc6 has neither ssize_t (we assume "int") nor off_t (assume "long") */

typedef unsigned char zzip_byte_t; // especially zlib decoding data

typedef       _zzip_off64_t     zzip_off64_t;
typedef       _zzip_off_t       zzip_off_t;
typedef       _zzip_size_t      zzip_size_t;      /* Some error here? */
typedef       _zzip_ssize_t     zzip_ssize_t;     /* See notes above! */

/* in <zzip/format.h> */
typedef struct zzip_disk64_trailer ZZIP_DISK64_TRAILER;
typedef struct zzip_disk_trailer ZZIP_DISK_TRAILER;
typedef struct zzip_file_trailer ZZIP_FILE_TRAILER;
typedef struct zzip_root_dirent  ZZIP_ROOT_DIRENT;
typedef struct zzip_file_header ZZIP_FILE_HEADER;
typedef struct zzip_disk_entry  ZZIP_DISK_ENTRY;
typedef struct zzip_extra_block ZZIP_EXTRA_BLOCK;



#endif

