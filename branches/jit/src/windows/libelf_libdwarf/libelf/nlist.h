/*
 * nlist.h - public header file for nlist(3).
 * Copyright (C) 1995 - 2006 Michael Riepe
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* @(#) $Id: nlist.h,v 1.10 2008/05/23 08:15:35 michael Exp $ */

#ifndef _NLIST_H
#define _NLIST_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct nlist {
    char*		n_name;
    long		n_value;
    short		n_scnum;
    unsigned short	n_type;
    char		n_sclass;
    char		n_numaux;
};

#if (__STDC__ + 0) || defined(__cplusplus) || defined(_WIN32)
extern int nlist(const char *__filename, struct nlist *__nl);
#else /* __STDC__ || defined(__cplusplus) */
extern int nlist();
#endif /* __STDC__ || defined(__cplusplus) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _NLIST_H */
