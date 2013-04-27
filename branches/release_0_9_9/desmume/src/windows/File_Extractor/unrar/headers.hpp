#ifndef _RAR_HEADERS_
#define _RAR_HEADERS_

#define  SIZEOF_MARKHEAD         7
#define  SIZEOF_OLDMHD           7
#define  SIZEOF_NEWMHD          13
#define  SIZEOF_OLDLHD          21
#define  SIZEOF_NEWLHD          32
#define  SIZEOF_SHORTBLOCKHEAD   7
#define  SIZEOF_SUBBLOCKHEAD    14
#define  SIZEOF_COMMHEAD        13

#define  UNP_VER                36

#define  MHD_VOLUME         0x0001
#define  MHD_COMMENT        0x0002
#define  MHD_SOLID          0x0008
#define  MHD_PASSWORD       0x0080

#define  LHD_SPLIT_BEFORE   0x0001
#define  LHD_SPLIT_AFTER    0x0002
#define  LHD_PASSWORD       0x0004
#define  LHD_COMMENT        0x0008
#define  LHD_SOLID          0x0010

#define  LHD_WINDOWMASK     0x00e0
#define  LHD_DIRECTORY      0x00e0

#define  LHD_LARGE          0x0100
#define  LHD_UNICODE        0x0200
#define  LHD_SALT           0x0400
#define  LHD_EXTTIME        0x1000

#define  LONG_BLOCK         0x8000

enum HEADER_TYPE {
	MARK_HEAD=0x72,MAIN_HEAD=0x73,FILE_HEAD=0x74,COMM_HEAD=0x75,AV_HEAD=0x76,
	SUB_HEAD=0x77,PROTECT_HEAD=0x78,SIGN_HEAD=0x79,NEWSUB_HEAD=0x7a,
	ENDARC_HEAD=0x7b
};

enum HOST_SYSTEM {
	HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
	HOST_BEOS=5,HOST_MAX
};

struct OldMainHeader
{
	byte Mark[4];
	ushort HeadSize;
	byte Flags;
};


struct OldFileHeader
{
	uint PackSize;
	uint UnpSize;
	ushort FileCRC;
	ushort HeadSize;
	uint FileTime;
	byte FileAttr;
	byte Flags;
	byte UnpVer;
	byte NameSize;
	byte Method;
};


struct MarkHeader
{
	byte Mark[7];
};


struct BaseBlock
{
	ushort HeadCRC;
	HEADER_TYPE HeadType;//byte
	ushort Flags;
	ushort HeadSize;
};

struct BlockHeader:BaseBlock
{
	union {
		uint DataSize;
		uint PackSize;
	};
};


struct MainHeader:BaseBlock
{
	ushort HighPosAV;
	uint PosAV;
};

#define SALT_SIZE     8

struct FileHeader:BlockHeader
{
	uint UnpSize;
	byte HostOS;
	uint FileCRC;
	uint FileTime;
	byte UnpVer;
	byte Method;
	ushort NameSize;
	union {
		uint FileAttr;
		uint SubFlags;
	};
/* optional */
	uint HighPackSize;
	uint HighUnpSize;
/* names */
	char FileName[NM*4]; // *4 to avoid using lots of stack in arcread
	wchar FileNameW[NM];
/* optional */
	byte Salt[SALT_SIZE];

	RarTime mtime;
/* dummy */
	Int64 FullPackSize;
	Int64 FullUnpSize;
};

// SubBlockHeader and its successors were used in RAR 2.x format.
// RAR 3.x uses FileHeader with NEWSUB_HEAD HeadType for subblocks.
struct SubBlockHeader:BlockHeader
{
	ushort SubType;
	byte Level;
};

struct ProtectHeader:BlockHeader
{
	byte Version;
	ushort RecSectors;
	uint TotalBlocks;
	byte Mark[8];
};

#endif
