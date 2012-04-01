/*
 * elf_repl.h - public header file for systems that lack it.
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

/* @(#) $Id: elf_repl.h,v 1.22 2009/11/01 13:04:19 michael Exp $ */

/*
 * NEVER INCLUDE THIS FILE DIRECTLY - USE <libelf.h> INSTEAD!
 */

#ifndef _ELF_REPL_H
#define _ELF_REPL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Scalar data types
 */
typedef __libelf_u32_t		Elf32_Addr;
typedef __libelf_u16_t		Elf32_Half;
typedef __libelf_u32_t		Elf32_Off;
typedef __libelf_i32_t		Elf32_Sword;
typedef __libelf_u32_t		Elf32_Word;

#define ELF32_FSZ_ADDR		4
#define ELF32_FSZ_HALF		2
#define ELF32_FSZ_OFF		4
#define ELF32_FSZ_SWORD		4
#define ELF32_FSZ_WORD		4

#if __LIBELF64

typedef __libelf_u64_t		Elf64_Addr;
typedef __libelf_u16_t		Elf64_Half;
typedef __libelf_u64_t		Elf64_Off;
typedef __libelf_i32_t		Elf64_Sword;
typedef __libelf_u32_t		Elf64_Word;
typedef __libelf_i64_t		Elf64_Sxword;
typedef __libelf_u64_t		Elf64_Xword;

#define ELF64_FSZ_ADDR		8
#define ELF64_FSZ_HALF		2
#define ELF64_FSZ_OFF		8
#define ELF64_FSZ_SWORD		4
#define ELF64_FSZ_WORD		4
#define ELF64_FSZ_SXWORD	8
#define ELF64_FSZ_XWORD		8

/*
 * Blame Sun for this...
 */
typedef __libelf_u64_t		Elf64_Lword;
typedef __libelf_u64_t		Elf32_Lword;

#endif /* __LIBELF64 */

/*
 * ELF header
 */
#define EI_NIDENT	16

typedef struct {
    unsigned char	e_ident[EI_NIDENT];
    Elf32_Half		e_type;
    Elf32_Half		e_machine;
    Elf32_Word		e_version;
    Elf32_Addr		e_entry;
    Elf32_Off		e_phoff;
    Elf32_Off		e_shoff;
    Elf32_Word		e_flags;
    Elf32_Half		e_ehsize;
    Elf32_Half		e_phentsize;
    Elf32_Half		e_phnum;
    Elf32_Half		e_shentsize;
    Elf32_Half		e_shnum;
    Elf32_Half		e_shstrndx;
} Elf32_Ehdr;

#if __LIBELF64
typedef struct {
    unsigned char	e_ident[EI_NIDENT];
    Elf64_Half		e_type;
    Elf64_Half		e_machine;
    Elf64_Word		e_version;
    Elf64_Addr		e_entry;
    Elf64_Off		e_phoff;
    Elf64_Off		e_shoff;
    Elf64_Word		e_flags;
    Elf64_Half		e_ehsize;
    Elf64_Half		e_phentsize;
    Elf64_Half		e_phnum;
    Elf64_Half		e_shentsize;
    Elf64_Half		e_shnum;
    Elf64_Half		e_shstrndx;
} Elf64_Ehdr;
#endif /* __LIBELF64 */

/*
 * e_ident
 */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_OSABI	7
#define EI_ABIVERSION	8
#define EI_PAD		9

#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'
#define ELFMAG		"\177ELF"
#define SELFMAG		4

/*
 * e_ident[EI_CLASS]
 */
#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2
#define ELFCLASSNUM	3

/*
 * e_ident[EI_DATA]
 */
#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2
#define ELFDATANUM	3

/*
 * e_ident[EI_OSABI]
 */
#define ELFOSABI_NONE		0	/* No extensions or unspecified */
#define ELFOSABI_SYSV		ELFOSABI_NONE
#define ELFOSABI_HPUX		1	/* Hewlett-Packard HP-UX */
#define ELFOSABI_NETBSD		2	/* NetBSD */
#define ELFOSABI_LINUX		3	/* Linux */
#define ELFOSABI_SOLARIS	6	/* Sun Solaris */
#define ELFOSABI_AIX		7	/* AIX */
#define ELFOSABI_IRIX		8	/* IRIX */
#define ELFOSABI_FREEBSD	9	/* FreeBSD */
#define ELFOSABI_TRU64		10	/* Compaq TRU64 UNIX */
#define ELFOSABI_MODESTO	11	/* Novell Modesto */
#define ELFOSABI_OPENBSD	12	/* Open BSD */
#define ELFOSABI_OPENVMS	13	/* Open VMS */
#define ELFOSABI_NSK		14	/* Hewlett-Packard Non-Stop Kernel */
#define ELFOSABI_AROS	 	15 	/* Amiga Research OS */
/* these are probably obsolete: */
#define ELFOSABI_ARM		97	/* ARM */
#define ELFOSABI_STANDALONE	255	/* standalone (embedded) application */


/*
 * e_type
 */
#define ET_NONE		0
#define ET_REL		1
#define ET_EXEC		2
#define ET_DYN		3
#define ET_CORE		4
#define ET_NUM		5
#define ET_LOOS		0xfe00
#define ET_HIOS		0xfeff
#define ET_LOPROC	0xff00
#define ET_HIPROC	0xffff

/*
 * e_machine
 */
#define EM_NONE		0	/* No machine */
#define EM_M32		1	/* AT&T WE 32100 */
#define EM_SPARC	2	/* SPARC */
#define EM_386		3	/* Intel 80386 */
#define EM_68K		4	/* Motorola 68000 */
#define EM_88K		5	/* Motorola 88000 */
#define EM_486		6	/* Intel i486 (DO NOT USE THIS ONE) */
#define EM_860		7	/* Intel 80860 */
#define EM_MIPS		8	/* MIPS I Architecture */
#define EM_S370		9	/* IBM System/370 Processor */
#define EM_MIPS_RS3_LE	10	/* MIPS RS3000 Little-endian */
#define EM_SPARC64	11	/* SPARC 64-bit */
#define EM_PARISC	15	/* Hewlett-Packard PA-RISC */
#define EM_VPP500	17	/* Fujitsu VPP500 */
#define EM_SPARC32PLUS	18	/* Enhanced instruction set SPARC */
#define EM_960		19	/* Intel 80960 */
#define EM_PPC		20	/* PowerPC */
#define EM_PPC64	21	/* 64-bit PowerPC */
#define EM_S390		22	/* IBM System/390 Processor */
#define EM_V800		36	/* NEC V800 */
#define EM_FR20		37	/* Fujitsu FR20 */
#define EM_RH32		38	/* TRW RH-32 */
#define EM_RCE		39	/* Motorola RCE */
#define EM_ARM		40	/* Advanced RISC Machines ARM */
#define EM_ALPHA	41	/* Digital Alpha */
#define EM_SH		42	/* Hitachi SH */
#define EM_SPARCV9	43	/* SPARC Version 9 */
#define EM_TRICORE	44	/* Siemens TriCore embedded processor */
#define EM_ARC		45	/* Argonaut RISC Core, Argonaut Technologies Inc. */
#define EM_H8_300	46	/* Hitachi H8/300 */
#define EM_H8_300H	47	/* Hitachi H8/300H */
#define EM_H8S		48	/* Hitachi H8S */
#define EM_H8_500	49	/* Hitachi H8/500 */
#define EM_IA_64	50	/* Intel IA-64 processor architecture */
#define EM_MIPS_X	51	/* Stanford MIPS-X */
#define EM_COLDFIRE	52	/* Motorola ColdFire */
#define EM_68HC12	53	/* Motorola M68HC12 */
#define EM_MMA		54	/* Fujitsu MMA Multimedia Accelerator */
#define EM_PCP		55	/* Siemens PCP */
#define EM_NCPU		56	/* Sony nCPU embedded RISC processor */
#define EM_NDR1		57	/* Denso NDR1 microprocessor */
#define EM_STARCORE	58	/* Motorola Star*Core processor */
#define EM_ME16		59	/* Toyota ME16 processor */
#define EM_ST100	60	/* STMicroelectronics ST100 processor */
#define EM_TINYJ	61	/* Advanced Logic Corp. TinyJ embedded processor family */
#define EM_X86_64	62	/* AMD x86-64 architecture */
#define EM_AMD64	EM_X86_64
#define EM_PDSP		63	/* Sony DSP Processor */
#define EM_FX66		66	/* Siemens FX66 microcontroller */
#define EM_ST9PLUS	67	/* STMicroelectronics ST9+ 8/16 bit microcontroller */
#define EM_ST7		68	/* STMicroelectronics ST7 8-bit microcontroller */
#define EM_68HC16	69	/* Motorola MC68HC16 Microcontroller */
#define EM_68HC11	70	/* Motorola MC68HC11 Microcontroller */
#define EM_68HC08	71	/* Motorola MC68HC08 Microcontroller */
#define EM_68HC05	72	/* Motorola MC68HC05 Microcontroller */
#define EM_SVX		73	/* Silicon Graphics SVx */
#define EM_ST19		74	/* STMicroelectronics ST19 8-bit microcontroller */
#define EM_VAX		75	/* Digital VAX */
#define EM_CRIS		76	/* Axis Communications 32-bit embedded processor */
#define EM_JAVELIN	77	/* Infineon Technologies 32-bit embedded processor */
#define EM_FIREPATH	78	/* Element 14 64-bit DSP Processor */
#define EM_ZSP		79	/* LSI Logic 16-bit DSP Processor */
#define EM_MMIX		80	/* Donald Knuth's educational 64-bit processor */
#define EM_HUANY	81	/* Harvard University machine-independent object files */
#define EM_PRISM	82	/* SiTera Prism */
#define EM_AVR		83	/* Atmel AVR 8-bit microcontroller */
#define EM_FR30		84	/* Fujitsu FR30 */
#define EM_D10V		85	/* Mitsubishi D10V */
#define EM_D30V		86	/* Mitsubishi D30V */
#define EM_V850		87	/* NEC v850 */
#define EM_M32R		88	/* Mitsubishi M32R */
#define EM_MN10300	89	/* Matsushita MN10300 */
#define EM_MN10200	90	/* Matsushita MN10200 */
#define EM_PJ		91	/* picoJava */
#define EM_OPENRISC	92	/* OpenRISC 32-bit embedded processor */
#define EM_ARC_A5	93	/* ARC Cores Tangent-A5 */
#define EM_XTENSA	94	/* Tensilica Xtensa Architecture */
#define EM_VIDEOCORE	95	/* Alphamosaic VideoCore processor */
#define EM_TMM_GPP	96	/* Thompson Multimedia General Purpose Processor */
#define EM_NS32K	97	/* National Semiconductor 32000 series */
#define EM_TPC		98	/* Tenor Network TPC processor */
#define EM_SNP1K	99	/* Trebia SNP 1000 processor */
#define EM_ST200	100	/* STMicroelectronics (www.st.com) ST200 microcontroller */
#define EM_IP2K 	101 	/* Ubicom IP2xxx microcontroller family */
#define EM_MAX 		102 	/* MAX Processor */
#define EM_CR 		103 	/* National Semiconductor CompactRISC microprocessor */
#define EM_F2MC16 	104 	/* Fujitsu F2MC16 */
#define EM_MSP430 	105 	/* Texas Instruments embedded microcontroller msp430 */
#define EM_BLACKFIN	106 	/* Analog Devices Blackfin (DSP) processor */
#define EM_SE_C33 	107 	/* S1C33 Family of Seiko Epson processors */
#define EM_SEP 		108 	/* Sharp embedded microprocessor */
#define EM_ARCA 	109 	/* Arca RISC Microprocessor */
#define EM_UNICORE 	110 	/* Microprocessor series from PKU-Unity Ltd. and MPRC of Peking University */
#define EM_NUM		111

/*
 * e_ident[EI_VERSION], e_version
 */
#define EV_NONE		0
#define EV_CURRENT	1
#define EV_NUM		2

/*
 * Section header
 */
typedef struct {
    Elf32_Word		sh_name;
    Elf32_Word		sh_type;
    Elf32_Word		sh_flags;
    Elf32_Addr		sh_addr;
    Elf32_Off		sh_offset;
    Elf32_Word		sh_size;
    Elf32_Word		sh_link;
    Elf32_Word		sh_info;
    Elf32_Word		sh_addralign;
    Elf32_Word		sh_entsize;
} Elf32_Shdr;

#if __LIBELF64
typedef struct {
    Elf64_Word		sh_name;
    Elf64_Word		sh_type;
    Elf64_Xword		sh_flags;
    Elf64_Addr		sh_addr;
    Elf64_Off		sh_offset;
    Elf64_Xword		sh_size;
    Elf64_Word		sh_link;
    Elf64_Word		sh_info;
    Elf64_Xword		sh_addralign;
    Elf64_Xword		sh_entsize;
} Elf64_Shdr;
#endif /* __LIBELF64 */

/*
 * Special section indices
 */
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_LOOS	0xff20
#define SHN_HIOS	0xff3f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_XINDEX	0xffff
#define SHN_HIRESERVE	0xffff

/*
 * sh_type
 */
#define SHT_NULL		0
#define SHT_PROGBITS		1
#define SHT_SYMTAB		2
#define SHT_STRTAB		3
#define SHT_RELA		4
#define SHT_HASH		5
#define SHT_DYNAMIC		6
#define SHT_NOTE		7
#define SHT_NOBITS		8
#define SHT_REL			9
#define SHT_SHLIB		10
#define SHT_DYNSYM		11
#define SHT_INIT_ARRAY		14
#define SHT_FINI_ARRAY		15
#define SHT_PREINIT_ARRAY	16
#define SHT_GROUP		17
#define SHT_SYMTAB_SHNDX	18
#define SHT_NUM			19
#define SHT_LOOS		0x60000000
#define SHT_HIOS		0x6fffffff
#define SHT_LOPROC		0x70000000
#define SHT_HIPROC		0x7fffffff
#define SHT_LOUSER		0x80000000
#define SHT_HIUSER		0xffffffff

/*
 * Solaris extensions
 */
#define SHT_LOSUNW		0x6ffffff4
#define SHT_SUNW_dof		0x6ffffff4
#define SHT_SUNW_cap		0x6ffffff5
#define SHT_SUNW_SIGNATURE	0x6ffffff6
#define SHT_SUNW_ANNOTATE	0x6ffffff7
#define SHT_SUNW_DEBUGSTR	0x6ffffff8
#define SHT_SUNW_DEBUG		0x6ffffff9
#define SHT_SUNW_move		0x6ffffffa
#define SHT_SUNW_COMDAT		0x6ffffffb
#define SHT_SUNW_syminfo	0x6ffffffc
#define SHT_SUNW_verdef		0x6ffffffd
#define SHT_SUNW_verneed	0x6ffffffe
#define SHT_SUNW_versym		0x6fffffff
#define SHT_HISUNW		0x6fffffff

#define SHT_SPARC_GOTDATA	0x70000000
#define SHT_AMD64_UNWIND	0x70000001 

/*
 * GNU extensions
 */
#define SHT_GNU_verdef		0x6ffffffd
#define SHT_GNU_verneed		0x6ffffffe
#define SHT_GNU_versym		0x6fffffff

/*
 * sh_flags
 */
#define SHF_WRITE		0x1
#define SHF_ALLOC		0x2
#define SHF_EXECINSTR		0x4
#define SHF_MERGE		0x10
#define SHF_STRINGS		0x20
#define SHF_INFO_LINK		0x40
#define SHF_LINK_ORDER		0x80
#define SHF_OS_NONCONFORMING	0x100
#define SHF_GROUP		0x200
#define SHF_TLS			0x400
#define SHF_MASKOS		0x0ff00000
#define SHF_MASKPROC		0xf0000000

/*
 * Solaris extensions
 */
#define SHF_AMD64_LARGE		0x10000000
#define SHF_ORDERED		0x40000000
#define SHF_EXCLUDE		0x80000000

/*
 * Section group flags
 */
#define GRP_COMDAT		0x1
#define GRP_MASKOS		0x0ff00000
#define GRP_MASKPROC		0xf0000000

/*
 * Symbol table
 */
typedef struct {
    Elf32_Word		st_name;
    Elf32_Addr		st_value;
    Elf32_Word		st_size;
    unsigned char	st_info;
    unsigned char	st_other;
    Elf32_Half		st_shndx;
} Elf32_Sym;

#if __LIBELF64
typedef struct {
    Elf64_Word		st_name;
    unsigned char	st_info;
    unsigned char	st_other;
    Elf64_Half		st_shndx;
    Elf64_Addr		st_value;
    Elf64_Xword		st_size;
} Elf64_Sym;
#endif /* __LIBELF64 */

/*
 * Special symbol indices
 */
#define STN_UNDEF	0

/*
 * Macros for manipulating st_info
 */
#define ELF32_ST_BIND(i)	((i)>>4)
#define ELF32_ST_TYPE(i)	((i)&0xf)
#define ELF32_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))

#if __LIBELF64
#define ELF64_ST_BIND(i)	((i)>>4)
#define ELF64_ST_TYPE(i)	((i)&0xf)
#define ELF64_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))
#endif /* __LIBELF64 */

/*
 * Symbol binding
 */
#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2
#define STB_NUM		3
#define STB_LOOS	10
#define STB_HIOS	12
#define STB_LOPROC	13
#define STB_HIPROC	15

/*
 * Symbol types
 */
#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4
#define STT_COMMON	5
#define STT_TLS		6
#define STT_NUM		7
#define STT_LOOS	10
#define STT_HIOS	12
#define STT_LOPROC	13
#define STT_HIPROC	15

/*
 * Macros for manipulating st_other
 */
#define ELF32_ST_VISIBILITY(o)	((o)&0x3)
#if __LIBELF64
#define ELF64_ST_VISIBILITY(o)	((o)&0x3)
#endif /* __LIBELF64 */

/*
 * Symbol visibility
 */
#define STV_DEFAULT	0
#define STV_INTERNAL	1
#define STV_HIDDEN	2
#define STV_PROTECTED	3

/*
 * Relocation
 */
typedef struct {
    Elf32_Addr		r_offset;
    Elf32_Word		r_info;
} Elf32_Rel;

typedef struct {
    Elf32_Addr		r_offset;
    Elf32_Word		r_info;
    Elf32_Sword		r_addend;
} Elf32_Rela;

#if __LIBELF64
typedef struct {
    Elf64_Addr		r_offset;
    Elf64_Xword		r_info;
} Elf64_Rel;

typedef struct {
    Elf64_Addr		r_offset;
    Elf64_Xword		r_info;
    Elf64_Sxword	r_addend;
} Elf64_Rela;
#endif /* __LIBELF64 */

/*
 * Macros for manipulating r_info
 */
#define ELF32_R_SYM(i)		((i)>>8)
#define ELF32_R_TYPE(i)		((unsigned char)(i))
#define ELF32_R_INFO(s,t)	(((s)<<8)+(unsigned char)(t))

#if __LIBELF64
#define ELF64_R_SYM(i)		((Elf64_Xword)(i)>>32)
#define ELF64_R_TYPE(i)		((i)&0xffffffffL)
#define ELF64_R_INFO(s,t)	(((Elf64_Xword)(s)<<32)+((t)&0xffffffffL))
#endif /* __LIBELF64 */

/*
 * Note entry header
 */
typedef struct {
    Elf32_Word		n_namesz;	/* name size */
    Elf32_Word		n_descsz;	/* descriptor size */
    Elf32_Word		n_type;		/* descriptor type */
} Elf32_Nhdr;

#if __LIBELF64
/* Solaris and GNU use this layout.  Be compatible. */
/* XXX: Latest ELF specs say it's 64-bit!!! */
typedef struct {
    Elf64_Word		n_namesz;	/* name size */
    Elf64_Word		n_descsz;	/* descriptor size */
    Elf64_Word		n_type;		/* descriptor type */
} Elf64_Nhdr;
#endif /* __LIBELF64 */

/*
 * Well-known descriptor types for ET_CORE files
 */
#define NT_PRSTATUS	1
#define NT_PRFPREG	2
#define NT_PRPSINFO	3

/*
 * Program header
 */
typedef struct {
    Elf32_Word		p_type;
    Elf32_Off		p_offset;
    Elf32_Addr		p_vaddr;
    Elf32_Addr		p_paddr;
    Elf32_Word		p_filesz;
    Elf32_Word		p_memsz;
    Elf32_Word		p_flags;
    Elf32_Word		p_align;
} Elf32_Phdr;

#if __LIBELF64
typedef struct {
    Elf64_Word		p_type;
    Elf64_Word		p_flags;
    Elf64_Off		p_offset;
    Elf64_Addr		p_vaddr;
    Elf64_Addr		p_paddr;
    Elf64_Xword		p_filesz;
    Elf64_Xword		p_memsz;
    Elf64_Xword		p_align;
} Elf64_Phdr;
#endif /* __LIBELF64 */

/*
 * Special numbers
 */
#define PN_XNUM		0xffff

/*
 * p_type
 */
#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_TLS		7
#define PT_NUM		8
#define PT_LOOS		0x60000000
#define PT_HIOS		0x6fffffff
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7fffffff

/*
 * Solaris extensions
 */

#define PT_SUNW_UNWIND	0x6464e550
#define PT_LOSUNW	0x6ffffffa
#define PT_SUNWBSS	0x6ffffffa
#define PT_SUNWSTACK	0x6ffffffb
#define PT_SUNWDTRACE	0x6ffffffc
#define PT_SUNWCAP	0x6ffffffd
#define PT_HISUNW	0x6fffffff 

/*
 * p_flags
 */
#define PF_X		0x1
#define PF_W		0x2
#define PF_R		0x4
#define PF_MASKOS	0x0ff00000
#define PF_MASKPROC	0xf0000000

/*
 * Dynamic structure
 */
typedef struct {
    Elf32_Sword		d_tag;
    union {
	Elf32_Word	d_val;
	Elf32_Addr	d_ptr;
    } d_un;
} Elf32_Dyn;

#if __LIBELF64
typedef struct {
    Elf64_Sxword	d_tag;
    union {
	Elf64_Xword	d_val;
	Elf64_Addr	d_ptr;
    } d_un;
} Elf64_Dyn;
#endif /* __LIBELF64 */

/*
 * Dynamic array tags
 */
					/* d_un   exec	 shared */
#define DT_NULL			0	/* ign.   mand.	 mand. */
#define DT_NEEDED		1	/* d_val  opt.	 opt.  */
#define DT_PLTRELSZ		2	/* d_val  opt.	 opt.  */
#define DT_PLTGOT		3	/* d_ptr  opt.	 opt.  */
#define DT_HASH			4	/* d_ptr  mand.	 mand. */
#define DT_STRTAB		5	/* d_ptr  mand.	 mand. */
#define DT_SYMTAB		6	/* d_ptr  mand.	 mand. */
#define DT_RELA			7	/* d_ptr  mand.	 opt.  */
#define DT_RELASZ		8	/* d_val  mand.	 opt.  */
#define DT_RELAENT		9	/* d_val  mand.	 opt.  */
#define DT_STRSZ		10	/* d_val  mand.	 mand. */
#define DT_SYMENT		11	/* d_val  mand.	 mand. */
#define DT_INIT			12	/* d_ptr  opt.	 opt.  */
#define DT_FINI			13	/* d_ptr  opt.	 opt.  */
#define DT_SONAME		14	/* d_val  ign.	 opt.  */
#define DT_RPATH		15	/* d_val  opt.	 ign.  */
#define DT_SYMBOLIC		16	/* ign.   ign.	 opt.  */
#define DT_REL			17	/* d_ptr  mand.	 opt.  */
#define DT_RELSZ		18	/* d_val  mand.	 opt.  */
#define DT_RELENT		19	/* d_val  mand.	 opt.  */
#define DT_PLTREL		20	/* d_val  opt.	 opt.  */
#define DT_DEBUG		21	/* d_ptr  opt.	 ign.  */
#define DT_TEXTREL		22	/* ign.   opt.	 opt.  */
#define DT_JMPREL		23	/* d_ptr  opt.	 opt.  */
#define DT_BIND_NOW		24	/* ign.   opt.	 opt.  */
#define DT_INIT_ARRAY		25	/* d_ptr  opt.	 opt.  */
#define DT_FINI_ARRAY		26	/* d_ptr  opt.	 opt.  */
#define DT_INIT_ARRAYSZ		27	/* d_val  opt.	 opt.  */
#define DT_FINI_ARRAYSZ		28	/* d_val  opt.	 opt.  */
#define DT_RUNPATH		29	/* d_val  opt.	 opt.  */
#define DT_FLAGS		30	/* d_val  opt.	 opt.  */
#define DT_ENCODING		32	/* odd/even encoding rule starts here */
#define DT_PREINIT_ARRAY	32	/* d_ptr  opt.	 ign.  */
#define DT_PREINIT_ARRAYSZ	33	/* d_val  opt.	 ign.  */
#define DT_NUM			34
#define DT_LOOS			0x6000000D
#define DT_HIOS			0x6ffff000
#define DT_LOPROC		0x70000000
#define DT_HIPROC		0x7fffffff

/*
 * DT_FLAGS values
 */
#define DF_ORIGIN	0x1
#define DF_SYMBOLIC	0x2
#define DF_TEXTREL	0x4
#define DF_BIND_NOW	0x8
#define DF_STATIC_TLS	0x10

/*
 * Solaris extensions
 */
#define DT_VALRNGLO	0x6ffffd00
#define DT_CHECKSUM	0x6ffffdf8
#define DT_PLTPADSZ	0x6ffffdf9
#define DT_MOVEENT	0x6ffffdfa
#define DT_MOVESZ	0x6ffffdfb
#define DT_FEATURE_1	0x6ffffdfc
#define DT_POSFLAG_1	0x6ffffdfd
#define DT_SYMINSZ	0x6ffffdfe
#define DT_SYMINENT	0x6ffffdff
#define DT_VALRNGHI	0x6ffffdff

#define DT_ADDRRNGLO	0x6ffffe00
#define DT_CONFIG	0x6ffffefa
#define DT_DEPAUDIT	0x6ffffefb
#define DT_AUDIT	0x6ffffefc
#define DT_PLTPAD	0x6ffffefd
#define DT_MOVETAB	0x6ffffefe
#define DT_SYMINFO	0x6ffffeff
#define DT_ADDRRNGHI	0x6ffffeff

#define DT_RELACOUNT	0x6ffffff9
#define DT_RELCOUNT	0x6ffffffa
#define DT_FLAGS_1	0x6ffffffb
#define DT_VERDEF	0x6ffffffc
#define DT_VERDEFNUM	0x6ffffffd
#define DT_VERNEED	0x6ffffffe
#define DT_VERNEEDNUM	0x6fffffff

#define DT_AUXILIARY	0x7ffffffd
#define DT_USED		0x7ffffffe
#define DT_FILTER	0x7fffffff

/*
 * GNU extensions
 */
#define DT_VERSYM	0x6ffffff0

/*
 * DT_FEATURE_1 values
 */
#define DTF_1_PARINIT	0x1
#define DTF_1_CONFEXP	0x2

/*
 * DT_POSFLAG_1 values
 */
#define DF_P1_LAZYLOAD	0x1
#define DF_P1_GROUPPERM	0x2

/*
 * DT_FLAGS_1 values
 */
#define DF_1_NOW	0x00000001
#define DF_1_GLOBAL	0x00000002
#define DF_1_GROUP	0x00000004
#define DF_1_NODELETE	0x00000008
#define DF_1_LOADFLTR	0x00000010
#define DF_1_INITFIRST	0x00000020
#define DF_1_NOOPEN	0x00000040
#define DF_1_ORIGIN	0x00000080
#define DF_1_DIRECT	0x00000100
#define DF_1_TRANS	0x00000200
#define DF_1_INTERPOSE	0x00000400
#define DF_1_NODEFLIB	0x00000800
#define DF_1_NODUMP	0x00001000
#define DF_1_CONFALT	0x00002000
#define DF_1_ENDFILTEE	0x00004000
#define DF_1_DISPRELDNE	0x00008000
#define DF_1_DISPRELPND	0x00010000

/*
 * Syminfo structure
 */
typedef struct {
    Elf32_Half		si_boundto;
    Elf32_Half		si_flags;
} Elf32_Syminfo;

#if __LIBELF64
typedef struct {
    Elf64_Half		si_boundto;
    Elf64_Half		si_flags;
} Elf64_Syminfo;
#endif /* __LIBELF64 */

/*
 * Syminfo version (stored in unused first entry)
 */
#define SYMINFO_NONE	0
#define SYMINFO_CURRENT	1
#define SYMINFO_NUM	2

/*
 * si_boundto special values
 */
#define SYMINFO_BT_LOWRESERVE	0xff00
#define SYMINFO_BT_PARENT	0xfffe	/* bound to parent */
#define SYMINFO_BT_SELF		0xffff	/* bound to self */

/*
 * si_flags
 */
#define SYMINFO_FLG_DIRECT	0x01	/* bound to an object */
#define SYMINFO_FLG_PASSTHRU	0x02	/* pass-thru symbol */
#define SYMINFO_FLG_COPY	0x04	/* result of a copy relocation */
#define SYMINFO_FLG_LAZYLOAD	0x08	/* bound to lazy-loaded object */

/*
 * Version definitions
 */
typedef struct {
    Elf32_Half		vd_version;
    Elf32_Half		vd_flags;
    Elf32_Half		vd_ndx;
    Elf32_Half		vd_cnt;
    Elf32_Word		vd_hash;
    Elf32_Word		vd_aux;
    Elf32_Word		vd_next;
} Elf32_Verdef;

typedef struct {
    Elf32_Word		vda_name;
    Elf32_Word		vda_next;
} Elf32_Verdaux;

typedef struct {
    Elf32_Half		vn_version;
    Elf32_Half		vn_cnt;
    Elf32_Word		vn_file;
    Elf32_Word		vn_aux;
    Elf32_Word		vn_next;
} Elf32_Verneed;

typedef struct {
    Elf32_Word		vna_hash;
    Elf32_Half		vna_flags;
    Elf32_Half		vna_other;
    Elf32_Word		vna_name;
    Elf32_Word		vna_next;
} Elf32_Vernaux;

typedef Elf32_Half	Elf32_Versym;

#if __LIBELF64

typedef struct {
    Elf64_Half		vd_version;
    Elf64_Half		vd_flags;
    Elf64_Half		vd_ndx;
    Elf64_Half		vd_cnt;
    Elf64_Word		vd_hash;
    Elf64_Word		vd_aux;
    Elf64_Word		vd_next;
} Elf64_Verdef;

typedef struct {
    Elf64_Word		vda_name;
    Elf64_Word		vda_next;
} Elf64_Verdaux;

typedef struct {
    Elf64_Half		vn_version;
    Elf64_Half		vn_cnt;
    Elf64_Word		vn_file;
    Elf64_Word		vn_aux;
    Elf64_Word		vn_next;
} Elf64_Verneed;

typedef struct {
    Elf64_Word		vna_hash;
    Elf64_Half		vna_flags;
    Elf64_Half		vna_other;
    Elf64_Word		vna_name;
    Elf64_Word		vna_next;
} Elf64_Vernaux;

typedef Elf64_Half	Elf64_Versym;

#endif /* __LIBELF64 */

/*
 * vd_version
 */
#define VER_DEF_NONE	0
#define VER_DEF_CURRENT	1
#define VER_DEF_NUM	2

/*
 * vn_version
 */
#define VER_NEED_NONE		0
#define VER_NEED_CURRENT	1
#define VER_NEED_NUM		2

/*
 * vd_flags / vna_flags
 */
#define VER_FLG_BASE	0x1	/* vd_flags only */
#define VER_FLG_WEAK	0x2

/*
 * Elf*_Versym special values
 */
#define VER_NDX_LOCAL	0
#define VER_NDX_GLOBAL	1

/*
 * Solaris extensions
 */

/*
 * Move section
 */
#if __LIBELF64

typedef struct {
    Elf32_Lword		m_value;
    Elf32_Word		m_info;
    Elf32_Word		m_poffset;
    Elf32_Half		m_repeat;
    Elf32_Half		m_stride;
} Elf32_Move;

typedef struct {
    Elf64_Lword		m_value;
    Elf64_Xword		m_info;
    Elf64_Xword		m_poffset;
    Elf64_Half		m_repeat;
    Elf64_Half		m_stride;
} Elf64_Move;

#define ELF32_M_SYM(info)	((info)>>8)
#define ELF32_M_SIZE(info)	((unsigned char)(info))
#define ELF32_M_INFO(sym, sz)	(((sym)<<8)+(unsigned char)(sz))

#define ELF64_M_SYM(info)	((Elf64_Xword)(info)>>8)
#define ELF64_M_SIZE(info)	((unsigned char)(info))
#define ELF64_M_INFO(sym, sz)	(((Elf64_Xword)(sym)<<8)+(unsigned char)(sz))

#endif /* __LIBELF64 */

/*
 * Capabilities
 */

typedef struct {
    Elf32_Word      	c_tag;
    union {
	Elf32_Word      c_val;
	Elf32_Addr      c_ptr;
    } c_un;
} Elf32_Cap;

#if __LIBELF64

typedef struct {
    Elf64_Xword     	c_tag;
    union {
	Elf64_Xword     c_val;
	Elf64_Addr      c_ptr;
    } c_un;
} Elf64_Cap;

#endif /* __LIBELF64 */

#define CA_SUNW_NULL	0	/* c_un ignored */
#define CA_SUNW_HW_1	1	/* c_un.c_val */
#define CA_SUNW_SF_1	2	/* c_un.c_val */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ELF_REPL_H */
