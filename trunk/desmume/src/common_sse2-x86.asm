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

	TITLE	common_sse2-x86.asm
	.686P
	.XMM
	.model	flat
	.code

@memcpy_fast@12 PROC PUBLIC
		push	esi
		push	edi
		
		mov		esi, edx
		mov		edi, ecx
		mov		ecx, [esp+12]
		
		prefetchnta [esi]
		prefetchnta [esi+64]
		prefetchnta [esi+128]
		prefetchnta [esi+192]
		prefetchnta [esi+256]
		
		cmp		ecx, 40h		; 64 bytes
		jl		remain_loop		;not enought bytes
		
		mov edx, edi
		and edx, 15
		je _aligned

		mov eax, ecx
		sub edx, 16
		neg edx
		sub eax, edx
		mov ecx, edx
		rep	movsb
		mov ecx, eax             

_aligned:		
		mov		eax, ecx
		shr		eax, 6
		and		ecx, 63
		test	esi, 15
		je		aligned_copy_loop

ALIGN 8
unaligned_copy_loop:
		prefetchnta [esi+320]
		movups	xmm0, [esi]
		movups	xmm1, [esi+16]
		movntps	[edi], xmm0
		movntps	[edi+16], xmm1
		movups	xmm2, [esi+32]
		movups	xmm3, [esi+48]
		movntps	[edi+32], xmm2
		movntps	[edi+48], xmm3
		add		esi, 64
		add		edi, 64
		dec		eax
		jne		unaligned_copy_loop
		sfence 
		jmp		remain_loop

ALIGN 8
aligned_copy_loop:
		prefetchnta [esi+320]
		movaps	xmm0, [esi]
		movaps	xmm1, [esi+16]
		movntps	[edi], xmm0
		movntps	[edi+16], xmm1
		movaps	xmm2, [esi+32]
		movaps	xmm3, [esi+48]
		movntps	[edi+32], xmm2
		movntps	[edi+48], xmm3
		add		esi, 64
		add		edi, 64
		dec		eax
		jne		aligned_copy_loop
		sfence 

remain_loop:
		cmp		ecx, 3
		jg		remain_loop2
		rep		movsb
		pop		edi
		pop		esi
		ret		4
remain_loop2:
		mov		eax, ecx
		shr		ecx, 2
		rep		movsd
		test	al, 2
		je		skip_word
		movsw
skip_word:
		test	al, 1
		je		end_loop
		movsb

end_loop:
		pop edi
		pop esi
		ret	4
@memcpy_fast@12 ENDP

@GPU_copyLine@8 PROC PUBLIC
		prefetchnta [edx]
		prefetchnta [edx+64]
		prefetchnta [edx+128]
		prefetchnta [edx+192]
		prefetchnta [edx+256]
		mov		eax, 8
		
aligned_copy_loop:
		prefetchnta [edx+320]
		movaps	xmm0, [edx]
		movaps	xmm1, [edx+16]
		movntps	[ecx], xmm0
		movntps	[ecx+16], xmm1
		movaps	xmm2, [edx+32]
		movaps	xmm3, [edx+48]
		movntps	[ecx+32], xmm2
		movntps	[ecx+48], xmm3
		add		edx, 64
		add		ecx, 64
		dec		eax
		jne		aligned_copy_loop
		sfence 

		ret 0
@GPU_copyLine@8 ENDP

end

