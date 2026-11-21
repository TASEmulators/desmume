// Runs a compiled block dumped by jit_diff.exe --dump, with a fake CPU struct.
//
// The point is to separate "the JIT emitted wrong code" from "the harness feeds
// or reads it wrong": the block is executed standalone, with only the CPU
// pointer patched in, and its resulting flags byte is printed.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
struct FakeCpu
{
	unsigned char pad[0x08];
	unsigned int  instr_adr;     // 0x08
	unsigned int  next_instr;    // 0x0C
	unsigned int  R[16];         // 0x10 .. 0x4F
	unsigned int  psr;           // 0x50
};
#pragma pack(pop)

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "block.bin";
	const unsigned int r0 = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 16) : 0xFFFFFFFFu;
	const unsigned int psr = argc > 3 ? (unsigned int)strtoul(argv[3], NULL, 16) : 0xF00000D3u;

	FILE *f = fopen(path, "rb");
	if (!f) { printf("cannot open %s\n", path); return 2; }
	unsigned char buf[512];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);

	// The block starts with: 48 83 EC 18 (sub rsp, 24) then 48 B9 <imm64>.
	if (n < 16 || buf[0] != 0x48 || buf[1] != 0x83 || buf[4] != 0x48 || buf[5] != 0xB9)
	{
		printf("unexpected prologue (not sub rsp / mov rcx, imm64)\n");
		return 2;
	}

	void *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!code) { printf("VirtualAlloc failed\n"); return 2; }
	memcpy(code, buf, n);

	struct FakeCpu *fc = (struct FakeCpu *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!fc) { printf("VirtualAlloc failed\n"); return 2; }
	memset(fc, 0, 4096);
	*(uintptr_t *)((unsigned char *)code + 6) = (uintptr_t)fc;

	fc->R[0] = r0;
	fc->R[1] = 1;
	fc->psr = psr;
	fc->instr_adr = 0x02000210;
	fc->next_instr = 0;

	// Optional: overwrite the flag-capture sequence (setb/sets/sete, 11 bytes)
	// with pushfq / pop r10 / mov [rdx+0x60], r10 / nops, so the host EFLAGS
	// right after the rotate can be read back.
	if (argc > 4 && strcmp(argv[4], "--flags") == 0)
	{
		static const unsigned char patch[11] = {
			0x9C,                          // pushfq
			0x41, 0x5A,                    // pop r10
			0x4C, 0x89, 0x52, 0x60,        // mov [rdx+0x60], r10
			0x90, 0x90, 0x90, 0x90         // nop
		};
		unsigned char *c = (unsigned char *)code;
		for (size_t i = 0; i + 11 <= n; i++)
		{
			if (c[i] == 0x0F && c[i + 1] == 0x92 && c[i + 2] == 0xC1)
			{
				memcpy(c + i, patch, sizeof(patch));
				printf("patched flag capture at +0x%zX\n", i);
				break;
			}
		}
	}

	{
		typedef unsigned int (*Fn)(void);
		Fn entry = (Fn)code;
		unsigned int cycles = entry();
		unsigned int outR0 = fc->R[0];
		unsigned int outPsr = fc->psr;
		unsigned int flags = (outPsr >> 24) & 0xFFu;

		printf("in  : R0=%08X cpsr=%08X\n", r0, psr);
		printf("out : R0=%08X cpsr=%08X cycles=%u\n", outR0, outPsr, cycles);
		printf("      flags byte=%02X (N=%u Z=%u C=%u V=%u)\n",
			flags, (flags >> 7) & 1u, (flags >> 6) & 1u, (flags >> 5) & 1u, (flags >> 4) & 1u);
		if (argc > 4 && strcmp(argv[4], "--flags") == 0)
		{
			const unsigned int eflags = *(unsigned int *)((unsigned char *)fc + 0x60);
			printf("      eflags after rotate=%08X (SF=%u ZF=%u CF=%u OF=%u)\n",
				eflags, (eflags >> 7) & 1u, (eflags >> 6) & 1u, (eflags >> 0) & 1u, (eflags >> 11) & 1u);
		}

		if (argc > 4 && strcmp(argv[4], "--control") == 0)
		{
			// Control: the same rotate, hand assembled, register destination.
			//   mov eax,[rcx]; mov cl,1; ror eax,cl; pushfq; pop rax; ret
			static const unsigned char snippet[] = {
				0x8B, 0x01, 0xB1, 0x01, 0xD3, 0xC8, 0x9C, 0x58, 0xC3
			};
			void *code2 = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(code2, snippet, sizeof(snippet));
			typedef unsigned int (*Fn2)(unsigned int *);
			unsigned int v = r0;
			unsigned int fl = ((Fn2)code2)(&v);
			printf("      control: in=%08X out=%08X eflags=%08X (SF=%u ZF=%u CF=%u)\n",
				r0, v, fl, (fl >> 7) & 1u, (fl >> 6) & 1u, (fl >> 0) & 1u);

			// Control 2: test eax,eax with the sign bit set, to check that SF is
			// readable at all through this mechanism.
			static const unsigned char snippet2[] = {
				0xB8, 0x00, 0x00, 0x00, 0x80,   // mov eax, 0x80000000
				0x85, 0xC0,                     // test eax, eax
				0x9C, 0x58, 0xC3                // pushfq; pop rax; ret
			};
			void *code3 = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(code3, snippet2, sizeof(snippet2));
			unsigned int fl2 = ((Fn2)code3)(&v);
			printf("      control2 (test eax,eax 0x80000000): eflags=%08X (SF=%u ZF=%u)\n",
				fl2, (fl2 >> 7) & 1u, (fl2 >> 6) & 1u);

			// Control 3: the immediate-count form, ror eax,1
			static const unsigned char snippet3[] = {
				0x8B, 0x01,                     // mov eax, [rcx]
				0xC1, 0xC8, 0x01,               // ror eax, 1
				0x89, 0x01,                     // mov [rcx], eax
				0x9C, 0x58, 0xC3                // pushfq; pop rax; ret
			};
			void *code4 = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(code4, snippet3, sizeof(snippet3));
			unsigned int v3 = r0;
			unsigned int fl3 = ((Fn2)code4)(&v3);
			printf("      control3 (ror eax,1 imm): out=%08X eflags=%08X (SF=%u ZF=%u CF=%u)\n",
				v3, fl3, (fl3 >> 7) & 1u, (fl3 >> 6) & 1u, (fl3 >> 0) & 1u);

			// Control 4: read the flags through LAHF instead of PUSHFQ, in case
			// the push/pop path itself is the problem.
			static const unsigned char snippet4[] = {
				0x8B, 0x01,                     // mov eax, [rcx]
				0xC1, 0xC8, 0x01,               // ror eax, 1
				0x9F,                           // lahf
				0x0F, 0xB6, 0xC4,               // movzx eax, ah
				0x89, 0x41, 0x04,               // mov [rcx+4], eax
				0xC3                            // ret
			};
			void *code5 = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(code5, snippet4, sizeof(snippet4));
			unsigned int v4[2] = { r0, 0 };
			((Fn2)code5)(v4);
			printf("      control4 (lahf after ror eax,1): ah=%02X (SF=%u ZF=%u CF=%u)\n",
				v4[1], (v4[1] >> 7) & 1u, (v4[1] >> 6) & 1u, (v4[1] >> 0) & 1u);

			// Control 5: same shape but with SHL, which must also set SF from
			// bit 31 of its result (0x40000001 << 1 = 0x80000002).
			static const unsigned char snippet5[] = {
				0x8B, 0x01,                     // mov eax, [rcx]
				0xC1, 0xE0, 0x01,               // shl eax, 1
				0x89, 0x01,                     // mov [rcx], eax
				0x9F,                           // lahf
				0x0F, 0xB6, 0xC4,               // movzx eax, ah
				0x89, 0x41, 0x04,               // mov [rcx+4], eax
				0xC3                            // ret
			};
			void *code6 = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(code6, snippet5, sizeof(snippet5));
			unsigned int v5[2] = { 0x40000001u, 0 };
			((Fn2)code6)(v5);
			printf("      control5 (lahf after shl eax,1): out=%08X ah=%02X (SF=%u)\n",
				v5[0], v5[1], (v5[1] >> 7) & 1u);
		}
	}
	return 0;
}
