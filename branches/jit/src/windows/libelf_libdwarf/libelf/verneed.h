/*
 * verneed.h - copy versioning information.
 * Copyright (C) 2001 - 2006 Michael Riepe
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

#ifndef lint
static const char verneed_h_rcsid[] = "@(#) $Id: verneed.h,v 1.13 2008/05/23 08:15:35 michael Exp $";
#endif /* lint */

#if VER_NEED_CURRENT != 1
#error libelf currently does not support VER_NEED_CURRENT != 1
#endif /* VER_NEED_CURRENT != 1 */

#if TOFILE

static void
__store_vernaux(vernaux_ftype *dst, const vernaux_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u32L(dst->vna_hash,  src->vna_hash);
	__store_u16L(dst->vna_flags, src->vna_flags);
	__store_u16L(dst->vna_other, src->vna_other);
	__store_u32L(dst->vna_name,  src->vna_name);
	__store_u32L(dst->vna_next,  src->vna_next);
    }
    else {
	__store_u32M(dst->vna_hash,  src->vna_hash);
	__store_u16M(dst->vna_flags, src->vna_flags);
	__store_u16M(dst->vna_other, src->vna_other);
	__store_u32M(dst->vna_name,  src->vna_name);
	__store_u32M(dst->vna_next,  src->vna_next);
    }
}

static void
__store_verneed(verneed_ftype *dst, const verneed_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u16L(dst->vn_version, src->vn_version);
	__store_u16L(dst->vn_cnt,     src->vn_cnt);
	__store_u32L(dst->vn_file,    src->vn_file);
	__store_u32L(dst->vn_aux,     src->vn_aux);
	__store_u32L(dst->vn_next,    src->vn_next);
    }
    else {
	__store_u16M(dst->vn_version, src->vn_version);
	__store_u16M(dst->vn_cnt,     src->vn_cnt);
	__store_u32M(dst->vn_file,    src->vn_file);
	__store_u32M(dst->vn_aux,     src->vn_aux);
	__store_u32M(dst->vn_next,    src->vn_next);
    }
}

typedef vernaux_mtype		vernaux_stype;
typedef vernaux_ftype		vernaux_dtype;
typedef verneed_mtype		verneed_stype;
typedef verneed_ftype		verneed_dtype;
typedef align_mtype		verneed_atype;

#define copy_vernaux_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_vernaux_tmptodst(d, s, e)	__store_vernaux((d), (s), (e))
#define copy_verneed_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verneed_tmptodst(d, s, e)	__store_verneed((d), (s), (e))

#define translator_suffix	_tof

#else /* TOFILE */

static void
__load_vernaux(vernaux_mtype *dst, const vernaux_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vna_hash  = __load_u32L(src->vna_hash);
	dst->vna_flags = __load_u16L(src->vna_flags);
	dst->vna_other = __load_u16L(src->vna_other);
	dst->vna_name  = __load_u32L(src->vna_name);
	dst->vna_next  = __load_u32L(src->vna_next);
    }
    else {
	dst->vna_hash  = __load_u32M(src->vna_hash);
	dst->vna_flags = __load_u16M(src->vna_flags);
	dst->vna_other = __load_u16M(src->vna_other);
	dst->vna_name  = __load_u32M(src->vna_name);
	dst->vna_next  = __load_u32M(src->vna_next);
    }
}

static void
__load_verneed(verneed_mtype *dst, const verneed_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vn_version = __load_u16L(src->vn_version);
	dst->vn_cnt     = __load_u16L(src->vn_cnt);
	dst->vn_file    = __load_u32L(src->vn_file);
	dst->vn_aux     = __load_u32L(src->vn_aux);
	dst->vn_next    = __load_u32L(src->vn_next);
    }
    else {
	dst->vn_version = __load_u16M(src->vn_version);
	dst->vn_cnt     = __load_u16M(src->vn_cnt);
	dst->vn_file    = __load_u32M(src->vn_file);
	dst->vn_aux     = __load_u32M(src->vn_aux);
	dst->vn_next    = __load_u32M(src->vn_next);
    }
}

typedef vernaux_ftype		vernaux_stype;
typedef vernaux_mtype		vernaux_dtype;
typedef verneed_ftype		verneed_stype;
typedef verneed_mtype		verneed_dtype;
typedef align_ftype		verneed_atype;

#define copy_vernaux_srctotmp(d, s, e)	__load_vernaux((d), (s), (e))
#define copy_vernaux_tmptodst(d, s, e)	(*(d) = *(s))
#define copy_verneed_srctotmp(d, s, e)	__load_verneed((d), (s), (e))
#define copy_verneed_tmptodst(d, s, e)	(*(d) = *(s))

#define translator_suffix	_tom

#endif /* TOFILE */

#define cat3(a,b,c)	a##b##c
#define xlt3(p,e,s)	cat3(p,e,s)
#define xltprefix(x)	xlt3(x,_,class_suffix)
#define translator(x,e)	xlt3(xltprefix(_elf_##x),e,translator_suffix)

static size_t
xlt_verneed(unsigned char *dst, const unsigned char *src, size_t n, unsigned enc) {
    size_t off;

    if (sizeof(verneed_stype) != sizeof(verneed_dtype)
     || sizeof(vernaux_stype) != sizeof(vernaux_dtype)) {
	/* never happens for ELF v1 and Verneed v1 */
	seterr(ERROR_UNIMPLEMENTED);
	return (size_t)-1;
    }
    /* size translation shortcut */
    if (dst == NULL) {
	return n;
    }
    if (src == NULL) {
	seterr(ERROR_NULLBUF);
	return (size_t)-1;
    }
    off = 0;
    while (off + sizeof(verneed_stype) <= n) {
	const verneed_stype *svn;
	verneed_dtype *dvn;
	verneed_mtype vn;
	size_t acount;
	size_t aoff;

	/*
	 * check for proper alignment
	 */
	if (off % sizeof(verneed_atype)) {
	    seterr(ERROR_VERNEED_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * copy and check src
	 */
	svn = (verneed_stype*)(src + off);
	dvn = (verneed_dtype*)(dst + off);
	copy_verneed_srctotmp(&vn, svn, enc);
	if (vn.vn_version < 1
	 || vn.vn_version > VER_NEED_CURRENT) {
	    seterr(ERROR_VERNEED_VERSION);
	    return (size_t)-1;
	}
	if (vn.vn_cnt < 1
	 || vn.vn_aux == 0) {
	    seterr(ERROR_VERNEED_FORMAT);
	    return (size_t)-1;
	}
	copy_verneed_tmptodst(dvn, &vn, enc);
	/*
	 * copy aux array
	 */
	aoff = off + vn.vn_aux;
	for (acount = 0; acount < vn.vn_cnt; acount++) {
	    const vernaux_stype *svna;
	    vernaux_dtype *dvna;
	    vernaux_mtype vna;

	    /*
	     * are we still inside the buffer limits?
	     */
	    if (aoff + sizeof(vernaux_stype) > n) {
		break;
	    }
	    /*
	     * check for proper alignment
	     */
	    if (aoff % sizeof(verneed_atype)) {
		seterr(ERROR_VERNEED_FORMAT);
		return (size_t)-1;
	    }
	    /*
	     * copy and check src
	     */
	    svna = (vernaux_stype*)(src + aoff);
	    dvna = (vernaux_dtype*)(dst + aoff);
	    copy_vernaux_srctotmp(&vna, svna, enc);
	    copy_vernaux_tmptodst(dvna, &vna, enc);
	    /*
	     * advance to next vernaux
	     */
	    if (vna.vna_next == 0) {
		/* end of list */
		break;
	    }
	    aoff += vna.vna_next;
	}
	/*
	 * advance to next verneed
	 */
	if (vn.vn_next == 0) {
	    /* end of list */
	    break;
	}
	off += vn.vn_next;
    }
    return n;
}

size_t
translator(verneed,L11)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verneed(dst, src, n, ELFDATA2LSB);
}

size_t
translator(verneed,M11)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verneed(dst, src, n, ELFDATA2MSB);
}
