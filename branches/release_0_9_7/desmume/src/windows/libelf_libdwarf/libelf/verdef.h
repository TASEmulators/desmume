/*
 * verdef.h - copy versioning information.
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
static const char verdef_h_rcsid[] = "@(#) $Id: verdef.h,v 1.13 2008/05/23 08:15:35 michael Exp $";
#endif /* lint */

#if VER_DEF_CURRENT != 1
#error libelf currently does not support VER_DEF_CURRENT != 1
#endif /* VER_DEF_CURRENT != 1 */

#if TOFILE

static void
__store_verdaux(verdaux_ftype *dst, const verdaux_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u32L(dst->vda_name, src->vda_name);
	__store_u32L(dst->vda_next, src->vda_next);
    }
    else {
	__store_u32M(dst->vda_name, src->vda_name);
	__store_u32M(dst->vda_next, src->vda_next);
    }
}

static void
__store_verdef(verdef_ftype *dst, const verdef_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u16L(dst->vd_version, src->vd_version);
	__store_u16L(dst->vd_flags,   src->vd_flags);
	__store_u16L(dst->vd_ndx,     src->vd_ndx);
	__store_u16L(dst->vd_cnt,     src->vd_cnt);
	__store_u32L(dst->vd_hash,    src->vd_hash);
	__store_u32L(dst->vd_aux,     src->vd_aux);
	__store_u32L(dst->vd_next,    src->vd_next);
    }
    else {
	__store_u16M(dst->vd_version, src->vd_version);
	__store_u16M(dst->vd_flags,   src->vd_flags);
	__store_u16M(dst->vd_ndx,     src->vd_ndx);
	__store_u16M(dst->vd_cnt,     src->vd_cnt);
	__store_u32M(dst->vd_hash,    src->vd_hash);
	__store_u32M(dst->vd_aux,     src->vd_aux);
	__store_u32M(dst->vd_next,    src->vd_next);
    }
}

typedef verdaux_mtype		verdaux_stype;
typedef verdaux_ftype		verdaux_dtype;
typedef verdef_mtype		verdef_stype;
typedef verdef_ftype		verdef_dtype;
typedef align_mtype		verdef_atype;

#define copy_verdaux_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verdaux_tmptodst(d, s, e)	__store_verdaux((d), (s), (e))
#define copy_verdef_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verdef_tmptodst(d, s, e)	__store_verdef((d), (s), (e))

#define translator_suffix	_tof

#else /* TOFILE */

static void
__load_verdaux(verdaux_mtype *dst, const verdaux_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vda_name = __load_u32L(src->vda_name);
	dst->vda_next = __load_u32L(src->vda_next);
    }
    else {
	dst->vda_name = __load_u32M(src->vda_name);
	dst->vda_next = __load_u32M(src->vda_next);
    }
}

static void
__load_verdef(verdef_mtype *dst, const verdef_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vd_version = __load_u16L(src->vd_version);
	dst->vd_flags   = __load_u16L(src->vd_flags);
	dst->vd_ndx     = __load_u16L(src->vd_ndx);
	dst->vd_cnt     = __load_u16L(src->vd_cnt);
	dst->vd_hash    = __load_u32L(src->vd_hash);
	dst->vd_aux     = __load_u32L(src->vd_aux);
	dst->vd_next    = __load_u32L(src->vd_next);
    }
    else {
	dst->vd_version = __load_u16M(src->vd_version);
	dst->vd_flags   = __load_u16M(src->vd_flags);
	dst->vd_ndx     = __load_u16M(src->vd_ndx);
	dst->vd_cnt     = __load_u16M(src->vd_cnt);
	dst->vd_hash    = __load_u32M(src->vd_hash);
	dst->vd_aux     = __load_u32M(src->vd_aux);
	dst->vd_next    = __load_u32M(src->vd_next);
    }
}

typedef verdaux_ftype		verdaux_stype;
typedef verdaux_mtype		verdaux_dtype;
typedef verdef_ftype		verdef_stype;
typedef verdef_mtype		verdef_dtype;
typedef align_ftype		verdef_atype;

#define copy_verdaux_srctotmp(d, s, e)	__load_verdaux((d), (s), (e))
#define copy_verdaux_tmptodst(d, s, e)	(*(d) = *(s))
#define copy_verdef_srctotmp(d, s, e)	__load_verdef((d), (s), (e))
#define copy_verdef_tmptodst(d, s, e)	(*(d) = *(s))

#define translator_suffix	_tom

#endif /* TOFILE */

#define cat3(a,b,c)	a##b##c
#define xlt3(p,e,s)	cat3(p,e,s)
#define xltprefix(x)	xlt3(x,_,class_suffix)
#define translator(x,e)	xlt3(xltprefix(_elf_##x),e,translator_suffix)

static size_t
xlt_verdef(unsigned char *dst, const unsigned char *src, size_t n, unsigned enc) {
    size_t off;

    if (sizeof(verdef_stype) != sizeof(verdef_dtype)
     || sizeof(verdaux_stype) != sizeof(verdaux_dtype)) {
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
    while (off + sizeof(verdef_stype) <= n) {
	const verdef_stype *svd;
	verdef_dtype *dvd;
	verdef_mtype vd;
	size_t acount;
	size_t aoff;

	/*
	 * check for proper alignment
	 */
	if (off % sizeof(verdef_atype)) {
	    seterr(ERROR_VERDEF_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * copy and check src
	 */
	svd = (verdef_stype*)(src + off);
	dvd = (verdef_dtype*)(dst + off);
	copy_verdef_srctotmp(&vd, svd, enc);
	if (vd.vd_version < 1
	 || vd.vd_version > VER_DEF_CURRENT) {
	    seterr(ERROR_VERDEF_VERSION);
	    return (size_t)-1;
	}
	if (vd.vd_cnt < 1
	 || vd.vd_aux == 0) {
	    seterr(ERROR_VERDEF_FORMAT);
	    return (size_t)-1;
	}
	copy_verdef_tmptodst(dvd, &vd, enc);
	/*
	 * copy aux array
	 */
	aoff = off + vd.vd_aux;
	for (acount = 0; acount < vd.vd_cnt; acount++) {
	    const verdaux_stype *svda;
	    verdaux_dtype *dvda;
	    verdaux_mtype vda;

	    /*
	     * are we still inside the buffer limits?
	     */
	    if (aoff + sizeof(verdaux_stype) > n) {
		break;
	    }
	    /*
	     * check for proper alignment
	     */
	    if (aoff % sizeof(verdef_atype)) {
		seterr(ERROR_VERDEF_FORMAT);
		return (size_t)-1;
	    }
	    /*
	     * copy and check src
	     */
	    svda = (verdaux_stype*)(src + aoff);
	    dvda = (verdaux_dtype*)(dst + aoff);
	    copy_verdaux_srctotmp(&vda, svda, enc);
	    copy_verdaux_tmptodst(dvda, &vda, enc);
	    /*
	     * advance to next verdaux
	     */
	    if (vda.vda_next == 0) {
		/* end of list */
		break;
	    }
	    aoff += vda.vda_next;
	}
	/*
	 * advance to next verdef
	 */
	if (vd.vd_next == 0) {
	    /* end of list */
	    break;
	}
	off += vd.vd_next;
    }
    return n;
}

size_t
translator(verdef,L11)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verdef(dst, src, n, ELFDATA2LSB);
}

size_t
translator(verdef,M11)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verdef(dst, src, n, ELFDATA2MSB);
}
