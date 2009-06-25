;
;	Copyright (C) 2006 yopyop
;	Copyright (C) 2008 CrazyMax
;
;    This file is part of DeSmuME
;
;    DeSmuME is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    DeSmuME is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with DeSmuME; if not, write to the Free Software
;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	TITLE	matrix_sse2-x86.asm
	.686P
	.XMM
	.model	flat
	.code
	
@_sse2_MatrixMultVec4x4@8 PROC PUBLIC
		movaps	xmm4, XMMWORD PTR [edx]
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		pshufd	xmm7, xmm4, 11111111b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4, XMMWORD PTR [ecx]
		mulps	xmm5, XMMWORD PTR [ecx+16]
		mulps	xmm6, XMMWORD PTR [ecx+32]
		mulps	xmm7, XMMWORD PTR [ecx+48]
		addps	xmm4, xmm5
		addps	xmm4, xmm6
		addps	xmm4, xmm7
		movaps	XMMWORD PTR [edx], xmm4
		ret		0
@_sse2_MatrixMultVec4x4@8 ENDP

@_sse2_MatrixMultVec3x3@8 PROC PUBLIC
		movaps	xmm4, XMMWORD PTR [edx]
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4, XMMWORD PTR [ecx]
		mulps	xmm5, XMMWORD PTR [ecx+16]
		mulps	xmm6, XMMWORD PTR [ecx+32]
		addps	xmm4, xmm5
		addps	xmm4, xmm6
		movaps	XMMWORD PTR [edx], xmm4
		ret		0
@_sse2_MatrixMultVec3x3@8 ENDP

@_sse2_MatrixMultiply@8 PROC PUBLIC
		movaps	xmm0, XMMWORD PTR [ecx]
		movaps	xmm1, XMMWORD PTR [ecx+16]
		movaps	xmm2, XMMWORD PTR [ecx+32]
		movaps	xmm3, XMMWORD PTR [ecx+48]
		movaps	xmm4, XMMWORD PTR [edx]			; r00, r01, r02, r03
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		pshufd	xmm7, xmm4, 11111111b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4,xmm0
		mulps	xmm5,xmm1
		mulps	xmm6,xmm2
		mulps	xmm7,xmm3
		addps	xmm4,xmm5
		addps	xmm4,xmm6
		addps	xmm4,xmm7
		movaps	XMMWORD PTR [ecx],xmm4

		movaps	xmm4, XMMWORD PTR [edx+16]		; r04, r05, r06, r07
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		pshufd	xmm7, xmm4, 11111111b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4,xmm0
		mulps	xmm5,xmm1
		mulps	xmm6,xmm2
		mulps	xmm7,xmm3
		addps	xmm4,xmm5
		addps	xmm4,xmm6
		addps	xmm4,xmm7
		movaps	XMMWORD PTR [ecx+16],xmm4

		movaps	xmm4, XMMWORD PTR [edx+32]		; r08, r09, r10, r11
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		pshufd	xmm7, xmm4, 11111111b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4,xmm0
		mulps	xmm5,xmm1
		mulps	xmm6,xmm2
		mulps	xmm7,xmm3
		addps	xmm4,xmm5
		addps	xmm4,xmm6
		addps	xmm4,xmm7
		movaps	XMMWORD PTR [ecx+32],xmm4

		movaps	xmm4, XMMWORD PTR [edx+48]		; r12, r13, r14, r15
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		pshufd	xmm7, xmm4, 11111111b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4,xmm0
		mulps	xmm5,xmm1
		mulps	xmm6,xmm2
		mulps	xmm7,xmm3
		addps	xmm4,xmm5
		addps	xmm4,xmm6
		addps	xmm4,xmm7
		movaps	XMMWORD PTR [ecx+48],xmm4

		ret		0
@_sse2_MatrixMultiply@8 ENDP

@_sse2_MatrixTranslate@8 PROC PUBLIC
		movaps	xmm4, XMMWORD PTR [edx]
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4, XMMWORD PTR [ecx]
		mulps	xmm5, XMMWORD PTR [ecx+16]
		mulps	xmm6, XMMWORD PTR [ecx+32]
		addps	xmm4, xmm5
		addps	xmm4, xmm6
		addps	xmm4, XMMWORD PTR [ecx+48]
		movaps	XMMWORD PTR [ecx+48], xmm4
		ret		0
@_sse2_MatrixTranslate@8 ENDP

@_sse2_MatrixScale@8 PROC PUBLIC
		movaps	xmm4, XMMWORD PTR [edx]
		pshufd	xmm5, xmm4, 01010101b
		pshufd	xmm6, xmm4, 10101010b
		shufps	xmm4, xmm4, 00000000b
		mulps	xmm4, XMMWORD PTR [ecx]
		mulps	xmm5, XMMWORD PTR [ecx+16]
		mulps	xmm6, XMMWORD PTR [ecx+32]
		movaps	XMMWORD PTR [ecx], xmm4
		movaps	XMMWORD PTR [ecx+16], xmm5
		movaps	XMMWORD PTR [ecx+32], xmm6
		ret		0
@_sse2_MatrixScale@8 ENDP

end

