#define XSFDRIVER_VERSIONS "0.22"
#define XSFDRIVER_MODULENAME "vio2sf.bin"
#define XSFDRIVER_ENTRYNAME "XSFSetup"
#define XSFDRIVER_ISSYSTEMTAG(taglen, tag) (((taglen) > 0) && ((tag)[0] == '_') && !(((taglen) > 8) && _strnicmp(tag, "_vio2sf_", 8)))
#define XSFDRIVER_GUID1 { 0xcfa2ca5c, 0xd9a3, 0x49a6, { 0x93, 0x31, 0xf4, 0x09, 0xa2, 0xc1, 0x67, 0xdc } } /* {CFA2CA5C-D9A3-49a6-9331-F409A2C167DC} */
#define XSFDRIVER_GUID2 { 0x5ae3d5fd, 0x0f4b, 0x4ca8, { 0xa9, 0x87, 0xca, 0x60, 0xc0, 0x97, 0x8b, 0x6a } } /* {5AE3D5FD-0F4B-4ca8-A987-CA60C0978B6A} */
#define XSFDRIVER_CHANNELMAP { { 16, "SPU %2d" } , { 0, 0 } }
#define XSFDRIVER_SIMPLENAME "2SF Decoder"

#define DESMUME_VERSIONS "0.8.0"
#define DESMUME_COPYRIGHT "Copyright (C) 2006 yopyop\nCopyright (C) 2006-2007 DeSmuME team"

#define WINAMPPLUGIN_COPYRIGHT DESMUME_COPYRIGHT
#define WINAMPPLUGIN_NAME "2SF Decorder " XSFDRIVER_VERSIONS "/ DeSmuME v" DESMUME_VERSIONS " (x86)"
#define WINAMPPLUGIN_EXTS "2SF;MINI2SF\0Double Screen Sound Format files(*.2SF;*.MINI2SF)\0\0\0"
#define WINAMPPLUGIN_TAG_XSFBY "2sfby"

#define KBMEDIAPLUGIN_VERSION 22
#define KBMEDIAPLUGIN_COPYRIGHT DESMUME_COPYRIGHT
#define KBMEDIAPLUGIN_NAME "2SF plugin " XSFDRIVER_VERSIONS " / DeSmuME v" DESMUME_VERSIONS
#define KBMEDIAPLUGIN_EXTS(n)	\
	static const char n##_2sfext[] = ".2sf";	\
	static const char n##_mini2sfext[] = ".mini2sf";	\
	static const char * const (n) [] = {	\
		n##_2sfext,	\
		n##_mini2sfext,	\
		0,	\
	};

#define FOOBAR2000COMPONENT_NAME "2SF decoder / DeSmuME v" DESMUME_VERSIONS
#define FOOBAR2000COMPONENT_VERSION XSFDRIVER_VERSIONS
#define FOOBAR2000COMPONENT_ABOUT "DeSmuME v" DESMUME_VERSIONS "\n" DESMUME_COPYRIGHT "\n"
#define FOOBAR2000COMPONENT_TYPE "Double Screen Sound Format files"
#define FOOBAR2000COMPONENT_EXTS "*.2SF;*.MINI2SF"
#define FOOBAR2000COMPONENT_EXT_CHECK (!stricmp_utf8(p_extension,"2SF") || !stricmp_utf8(p_extension,"MINI2SF"))
#define FOOBAR2000COMPONENT_ENCODING "2sf"
