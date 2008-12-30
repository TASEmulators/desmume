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

	TITLE	common-x86.asm
	.686P
	.XMM
	.model	flat
	.code

@memcpy_fast@12 PROC PUBLIC
		push	esi
		push	edi		; cauntion: stack increased on 8
		
		mov		esi, edx
		mov		edi, ecx
		mov		ecx, [esp+12]
		
		cmp		ecx, 40h		; 64 bytes
		jl		remain_loop		;not enought bytes
		
		mov		eax, ecx
		shr		eax, 6
		and		ecx, 63

ALIGN 8		
loop_copy:
		movq		mm0, [esi]
		movq		mm1, [esi+ 8]
		movntq		[edi], mm0
		movntq		[edi+8], mm1
		movq		mm2, [esi+16]
		movq		mm3, [esi+24]
		movntq		[edi+16], mm2
		movntq		[edi+24], mm3
		movq		mm4, [esi+32]
		movq		mm5, [esi+40]
		movntq		[edi+32], mm4
		movntq		[edi+40], mm5
		movq		mm6, [esi+48]
		movq		mm7, [esi+56]
		movntq		[edi+48], mm6
		movntq		[edi+56], mm7
		add			esi, 64
		add			edi, 64
		dec			eax
		jne			loop_copy

		emms
		
remain_loop:
		rep			movsb
end_loop:
		pop edi
		pop esi
		ret	4
@memcpy_fast@12 ENDP

@GPU_copyLine@8 PROC PUBLIC
		mov		eax, 8
ALIGN 8		
loop_copy:
		movq		mm0, [edx]
		movq		mm1, [edx+8]
		movntq		[ecx], mm0
		movntq		[ecx+8], mm1
		movq		mm2, [edx+16]
		movq		mm3, [edx+24]
		movntq		[ecx+16], mm2
		movntq		[ecx+24], mm3
		movq		mm4, [edx+32]
		movq		mm5, [edx+40]
		movntq		[ecx+32], mm4
		movntq		[ecx+40], mm5
		movq		mm6, [edx+48]
		movq		mm7, [edx+56]
		movntq		[ecx+48], mm6
		movntq		[ecx+56], mm7
		add			edx, 64
		add			ecx, 64
		dec			eax
		jne			loop_copy

		emms	
		ret	0
@GPU_copyLine@8 ENDP

end
