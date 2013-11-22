/*
	Copyright (C) 2008-2010 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

//TODO - move this into ndssystem where it belongs probably

#include <string.h>
#include <string>
#include <stdarg.h>
#include "common.h"
#include <zlib.h>
#include <stdlib.h>

char *trim(char *s, int len)
{
	char *ptr = NULL;
	if (!s) return NULL;
	if (!*s) return s;
	
	if(len==-1)
		ptr = s + strlen(s) - 1;
	else ptr = s+len - 1;
	for (; (ptr >= s) && (!*ptr || isspace((u8)*ptr)) ; ptr--);
	ptr[1] = '\0';
	return s;
}

char *removeSpecialChars(char *s)
{
	char	*buf = s;
	if (!s) return NULL;
	if (!*s) return s;

	for (u32 i = 0; i < strlen(s); i++)
	{
		if (isspace((u8)s[i]) && (s[i] != 0x20))
			*buf = 0x20;
		else
			*buf = s[i];
		buf++;
	}
	*buf = 0;
	return s;
}

static MAKER makerCodes[] = {
	{ 0x3130, "Nintendo" },
	{ 0x3230, "Rocket Games, Ajinomoto" },
	{ 0x3330, "Imagineer-Zoom" },
	{ 0x3430, "Gray Matter?" },
	{ 0x3530, "Zamuse" },
	{ 0x3630, "Falcom" },
	{ 0x3730, "Enix?" },
	{ 0x3830, "Capcom" },
	{ 0x3930, "Hot B Co." },
	{ 0x4130, "Jaleco" },
	{ 0x4230, "Coconuts Japan" },
	{ 0x4330, "Coconuts Japan/G.X.Media" },
	{ 0x4430, "Micronet?" },
	{ 0x4530, "Technos" },
	{ 0x4630, "Mebio Software" },
	{ 0x4730, "Shouei System" },
	{ 0x4830, "Starfish" },
	{ 0x4A30, "Mitsui Fudosan/Dentsu" },
	{ 0x4C30, "Warashi Inc." },
	{ 0x4E30, "Nowpro" },
	{ 0x5030, "Game Village" },
	{ 0x3031, "?????????????" },
	{ 0x3231, "Infocom" },
	{ 0x3331, "Electronic Arts Japan" },
	{ 0x3531, "Cobra Team" },
	{ 0x3631, "Human/Field" },
	{ 0x3731, "KOEI" },
	{ 0x3831, "Hudson Soft" },
	{ 0x3931, "S.C.P." },
	{ 0x4131, "Yanoman" },
	{ 0x4331, "Tecmo Products" },
	{ 0x4431, "Japan Glary Business" },
	{ 0x4531, "Forum/OpenSystem" },
	{ 0x4631, "Virgin Games" },
	{ 0x4731, "SMDE" },
	{ 0x4A31, "Daikokudenki" },
	{ 0x5031, "Creatures Inc." },
	{ 0x5131, "TDK Deep Impresion" },
	{ 0x3032, "Destination Software, KSS" },
	{ 0x3132, "Sunsoft/Tokai Engineering??" },
	{ 0x3232, "POW, VR 1 Japan??" },
	{ 0x3332, "Micro World" },
	{ 0x3532, "San-X" },
	{ 0x3632, "Enix" },
	{ 0x3732, "Loriciel/Electro Brain" },
	{ 0x3832, "Kemco Japan" },
	{ 0x3932, "Seta" },
	{ 0x4132, "Culture Brain" },
	{ 0x4332, "Palsoft" },
	{ 0x4432, "Visit Co.,Ltd." },
	{ 0x4532, "Intec" },
	{ 0x4632, "System Sacom" },
	{ 0x4732, "Poppo" },
	{ 0x4832, "Ubisoft Japan" },
	{ 0x4A32, "Media Works" },
	{ 0x4B32, "NEC InterChannel" },
	{ 0x4C32, "Tam" },
	{ 0x4D32, "Jordan" },
	{ 0x4E32, "Smilesoft ???, Rocket ???" },
	{ 0x5132, "Mediakite" },
	{ 0x3033, "Viacom" },
	{ 0x3133, "Carrozzeria" },
	{ 0x3233, "Dynamic" },
	{ 0x3433, "Magifact" },
	{ 0x3533, "Hect" },
	{ 0x3633, "Codemasters" },
	{ 0x3733, "Taito/GAGA Communications" },
	{ 0x3833, "Laguna" },
	{ 0x3933, "Telstar Fun & Games, Event/Taito" },
	{ 0x4233, "Arcade Zone Ltd" },
	{ 0x4333, "Entertainment International/Empire Software?" },
	{ 0x4433, "Loriciel" },
	{ 0x4533, "Gremlin Graphics" },
	{ 0x4633, "K.Amusement Leasing Co." },
	{ 0x3034, "Seika Corp." },
	{ 0x3134, "Ubi Soft Entertainment" },
	{ 0x3234, "Sunsoft US?" },
	{ 0x3434, "Life Fitness" },
	{ 0x3634, "System 3" },
	{ 0x3734, "Spectrum Holobyte" },
	{ 0x3934, "IREM" },
	{ 0x4234, "Raya Systems" },
	{ 0x4334, "Renovation Products" },
	{ 0x4434, "Malibu Games" },
	{ 0x4634, "Eidos (was U.S. Gold <=1995)" },
	{ 0x4734, "Playmates Interactive?" },
	{ 0x4A34, "Fox Interactive" },
	{ 0x4B34, "Time Warner Interactive" },
	{ 0x5134, "Disney Interactive" },
	{ 0x5334, "Black Pearl" },
	{ 0x5534, "Advanced Productions" },
	{ 0x5834, "GT Interactive" },
	{ 0x5934, "RARE?" },
	{ 0x5A34, "Crave Entertainment" },
	{ 0x3035, "Absolute Entertainment" },
	{ 0x3135, "Acclaim" },
	{ 0x3235, "Activision" },
	{ 0x3335, "American Sammy" },
	{ 0x3435, "Take 2 Interactive (before it was GameTek)" },
	{ 0x3535, "Hi Tech" },
	{ 0x3635, "LJN LTD." },
	{ 0x3835, "Mattel" },
	{ 0x4135, "Mindscape, Red Orb Entertainment?" },
	{ 0x4235, "Romstar" },
	{ 0x4335, "Taxan" },
	{ 0x4435, "Midway (before it was Tradewest)" },
	{ 0x4635, "American Softworks" },
	{ 0x4735, "Majesco Sales Inc" },
	{ 0x4835, "3DO" },
	{ 0x4B35, "Hasbro" },
	{ 0x4C35, "NewKidCo" },
	{ 0x4D35, "Telegames" },
	{ 0x4E35, "Metro3D" },
	{ 0x5035, "Vatical Entertainment" },
	{ 0x5135, "LEGO Media" },
	{ 0x5335, "Xicat Interactive" },
	{ 0x5435, "Cryo Interactive" },
	{ 0x5735, "Red Storm Entertainment" },
	{ 0x5835, "Microids" },
	{ 0x5A35, "Conspiracy/Swing" },
	{ 0x3036, "Titus" },
	{ 0x3136, "Virgin Interactive" },
	{ 0x3236, "Maxis" },
	{ 0x3436, "LucasArts Entertainment" },
	{ 0x3736, "Ocean" },
	{ 0x3936, "Electronic Arts" },
	{ 0x4236, "Laser Beam" },
	{ 0x4536, "Elite Systems" },
	{ 0x4636, "Electro Brain" },
	{ 0x4736, "The Learning Company" },
	{ 0x4836, "BBC" },
	{ 0x4A36, "Software 2000" },
	{ 0x4C36, "BAM! Entertainment" },
	{ 0x4D36, "Studio 3" },
	{ 0x5136, "Classified Games" },
	{ 0x5336, "TDK Mediactive" },
	{ 0x5536, "DreamCatcher" },
	{ 0x5636, "JoWood Produtions" },
	{ 0x5736, "SEGA" },
	{ 0x5836, "Wannado Edition" },
	{ 0x5936, "LSP" },
	{ 0x5A36, "ITE Media" },
	{ 0x3037, "Infogrames" },
	{ 0x3137, "Interplay" },
	{ 0x3237, "JVC" },
	{ 0x3337, "Parker Brothers" },
	{ 0x3537, "Sales Curve" },
	{ 0x3837, "THQ" },
	{ 0x3937, "Accolade" },
	{ 0x4137, "Triffix Entertainment" },
	{ 0x4337, "Microprose Software" },
	{ 0x4437, "Universal Interactive, Sierra, Simon & Schuster?" },
	{ 0x4637, "Kemco" },
	{ 0x4737, "Rage Software" },
	{ 0x4837, "Encore" },
	{ 0x4A37, "Zoo" },
	{ 0x4B37, "BVM" },
	{ 0x4C37, "Simon & Schuster Interactive" },
	{ 0x4D37, "Asmik Ace Entertainment Inc./AIA" },
	{ 0x4E37, "Empire Interactive?" },
	{ 0x5137, "Jester Interactive" },
	{ 0x5437, "Scholastic" },
	{ 0x5537, "Ignition Entertainment" },
	{ 0x5737, "Stadlbauer" },
	{ 0x3038, "Misawa" },
	{ 0x3138, "Teichiku" },
	{ 0x3238, "Namco Ltd." },
	{ 0x3338, "LOZC" },
	{ 0x3438, "KOEI" },
	{ 0x3638, "Tokuma Shoten Intermedia" },
	{ 0x3738, "Tsukuda Original" },
	{ 0x3838, "DATAM-Polystar" },
	{ 0x4238, "Bulletproof Software" },
	{ 0x4338, "Vic Tokai Inc." },
	{ 0x4538, "Character Soft" },
	{ 0x4638, "I'Max" },
	{ 0x4738, "Saurus" },
	{ 0x4A38, "General Entertainment" },
	{ 0x4E38, "Success" },
	{ 0x5038, "SEGA Japan" },
	{ 0x3039, "Takara Amusement" },
	{ 0x3139, "Chun Soft" },
	{ 0x3239, "Video System, McO'River???" },
	{ 0x3339, "BEC" },
	{ 0x3539, "Varie" },
	{ 0x3639, "Yonezawa/S'pal" },
	{ 0x3739, "Kaneko" },
	{ 0x3939, "Victor Interactive Software, Pack in Video" },
	{ 0x4139, "Nichibutsu/Nihon Bussan" },
	{ 0x4239, "Tecmo" },
	{ 0x4339, "Imagineer" },
	{ 0x4639, "Nova" },
	{ 0x4739, "Den'Z" },
	{ 0x4839, "Bottom Up" },
	{ 0x4A39, "TGL" },
	{ 0x4C39, "Hasbro Japan?" },
	{ 0x4E39, "Marvelous Entertainment" },
	{ 0x5039, "Keynet Inc." },
	{ 0x5139, "Hands-On Entertainment" },
	{ 0x3041, "Telenet" },
	{ 0x3141, "Hori" },
	{ 0x3441, "Konami" },
	{ 0x3541, "K.Amusement Leasing Co." },
	{ 0x3641, "Kawada" },
	{ 0x3741, "Takara" },
	{ 0x3941, "Technos Japan Corp." },
	{ 0x4141, "JVC, Victor Musical Indutries" },
	{ 0x4341, "Toei Animation" },
	{ 0x4441, "Toho" },
	{ 0x4641, "Namco" },
	{ 0x4741, "Media Rings Corporation" },
	{ 0x4841, "J-Wing" },
	{ 0x4A41, "Pioneer LDC" },
	{ 0x4B41, "KID" },
	{ 0x4C41, "Mediafactory" },
	{ 0x5041, "Infogrames Hudson" },
	{ 0x5141, "Kiratto. Ludic Inc" },
	{ 0x3042, "Acclaim Japan" },
	{ 0x3142, "ASCII (was Nexoft?)" },
	{ 0x3242, "Bandai" },
	{ 0x3442, "Enix" },
	{ 0x3642, "HAL Laboratory" },
	{ 0x3742, "SNK" },
	{ 0x3942, "Pony Canyon" },
	{ 0x4142, "Culture Brain" },
	{ 0x4242, "Sunsoft" },
	{ 0x4342, "Toshiba EMI" },
	{ 0x4442, "Sony Imagesoft" },
	{ 0x4642, "Sammy" },
	{ 0x4742, "Magical" },
	{ 0x4842, "Visco" },
	{ 0x4A42, "Compile " },
	{ 0x4C42, "MTO Inc." },
	{ 0x4E42, "Sunrise Interactive" },
	{ 0x5042, "Global A Entertainment" },
	{ 0x5142, "Fuuki" },
	{ 0x3043, "Taito" },
	{ 0x3243, "Kemco" },
	{ 0x3343, "Square" },
	{ 0x3443, "Tokuma Shoten" },
	{ 0x3543, "Data East" },
	{ 0x3643, "Tonkin House	(was Tokyo Shoseki)" },
	{ 0x3843, "Koei" },
	{ 0x4143, "Konami/Ultra/Palcom" },
	{ 0x4243, "NTVIC/VAP" },
	{ 0x4343, "Use Co.,Ltd." },
	{ 0x4443, "Meldac" },
	{ 0x4543, "Pony Canyon" },
	{ 0x4643, "Angel, Sotsu Agency/Sunrise" },
	{ 0x4A43, "Boss" },
	{ 0x4743, "Yumedia/Aroma Co., Ltd" },
	{ 0x4B43, "Axela/Crea-Tech?" },
	{ 0x4C43, "Sekaibunka-Sha, Sumire kobo?, Marigul Management Inc.?" },
	{ 0x4D43, "Konami Computer Entertainment Osaka" },
	{ 0x5043, "Enterbrain" },
	{ 0x3044, "Taito/Disco" },
	{ 0x3144, "Sofel" },
	{ 0x3244, "Quest, Bothtec" },
	{ 0x3344, "Sigma, ?????" },
	{ 0x3444, "Ask Kodansha" },
	{ 0x3644, "Naxat" },
	{ 0x3744, "Copya System" },
	{ 0x3844, "Capcom Co., Ltd." },
	{ 0x3944, "Banpresto" },
	{ 0x4144, "TOMY" },
	{ 0x4244, "LJN Japan" },
	{ 0x4444, "NCS" },
	{ 0x4544, "Human Entertainment" },
	{ 0x4644, "Altron" },
	{ 0x4744, "Jaleco???" },
	{ 0x4844, "Gaps Inc." },
	{ 0x4C44, "????" },
	{ 0x4E44, "Elf" },
	{ 0x3045, "Jaleco" },
	{ 0x3145, "????" },
	{ 0x3245, "Yutaka" },
	{ 0x3345, "Varie" },
	{ 0x3445, "T&ESoft" },
	{ 0x3545, "Epoch" },
	{ 0x3745, "Athena" },
	{ 0x3845, "Asmik" },
	{ 0x3945, "Natsume" },
	{ 0x4145, "King Records" },
	{ 0x4245, "Atlus" },
	{ 0x4345, "Epic/Sony Records" },
	{ 0x4545, "IGS" },
	{ 0x4745, "Chatnoir" },
	{ 0x4845, "Right Stuff" },
	{ 0x4C45, "Spike" },
	{ 0x4D45, "Konami Computer Entertainment Tokyo" },
	{ 0x4E45, "Alphadream Corporation" },
	{ 0x3046, "A Wave" },
	{ 0x3146, "Motown Software" },
	{ 0x3246, "Left Field Entertainment" },
	{ 0x3346, "Extreme Ent. Grp." },
	{ 0x3446, "TecMagik" },
	{ 0x3946, "Cybersoft" },
	{ 0x4246, "Psygnosis" },
	{ 0x4546, "Davidson/Western Tech." },
	{ 0x3147, "PCCW Japan" },
	{ 0x3447, "KiKi Co Ltd" },
	{ 0x3547, "Open Sesame Inc???" },
	{ 0x3647, "Sims" },
	{ 0x3747, "Broccoli" },
	{ 0x3847, "Avex" },
	{ 0x3947, "D3 Publisher" },
	{ 0x4247, "Konami Computer Entertainment Japan" },
	{ 0x4447, "Square-Enix" },
	{ 0x4849, "Yojigen" },
};

#define makerNum sizeof(makerCodes) / sizeof(makerCodes[0])

std::string getDeveloperNameByID(u16 id)
{
	for (u32 i = 0; i < makerNum; i++)
	{
		if (makerCodes[i].code == id)
		{
			return makerCodes[i].name;
		}
	}
	return "Unknown";
}

// ===============================================================================
// PNG/BMP
// ===============================================================================
static int WritePNGChunk(FILE *fp, uint32 size, const char *type, const uint8 *data)
{
	uint32 crc;

	uint8 tempo[4];

	tempo[0]=size>>24;
	tempo[1]=size>>16;
	tempo[2]=size>>8;
	tempo[3]=size;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	if(fwrite(type,4,1,fp)!=1)
		return 0;

	if(size)
		if(fwrite(data,1,size,fp)!=size)
			return 0;

	crc = crc32(0,(uint8 *)type,4);
	if(size)
		crc = crc32(crc,data,size);

	tempo[0]=crc>>24;
	tempo[1]=crc>>16;
	tempo[2]=crc>>8;
	tempo[3]=crc;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	return 1;
}

int NDS_WritePNG(const char *fname, u8 *data)
{
	int x, y;
	int width=256;
	int height=192*2;
	u16 * bmp = (u16 *)data;
	FILE *pp=NULL;
	uint8 *compmem = NULL;
	uLongf compmemsize = (uLongf)( (height * (width + 1) * 3 * 1.001 + 1) + 12 );

	if(!(compmem=(uint8 *)malloc(compmemsize)))
		return 0;

	if(!(pp=fopen(fname, "wb")))
	{
		goto PNGerr;
	}
	{
		const uint8 header[8]={137,80,78,71,13,10,26,10};
		if(fwrite(header,8,1,pp)!=1)
			goto PNGerr;
	}

	{
		uint8 chunko[13];

		chunko[0] = width >> 24;		// Width
		chunko[1] = width >> 16;
		chunko[2] = width >> 8;
		chunko[3] = width;

		chunko[4] = height >> 24;		// Height
		chunko[5] = height >> 16;
		chunko[6] = height >> 8;
		chunko[7] = height;

		chunko[8]=8;				// 8 bits per sample(24 bits per pixel)
		chunko[9]=2;				// Color type; RGB triplet
		chunko[10]=0;				// compression: deflate
		chunko[11]=0;				// Basic adapative filter set(though none are used).
		chunko[12]=0;				// No interlace.

		if(!WritePNGChunk(pp,13,"IHDR",chunko))
			goto PNGerr;
	}

	{
		uint8 *tmp_buffer;
		uint8 *tmp_inc;
		tmp_inc = tmp_buffer = (uint8 *)malloc((width * 3 + 1) * height);

		for(y=0;y<height;y++)
		{
			*tmp_inc = 0;
			tmp_inc++;
			for(x=0;x<width;x++)
			{
				int r,g,b;
				u16 pixel = bmp[y*256+x];
				r = pixel>>10;
				pixel-=r<<10;
				g = pixel>>5;
				pixel-=g<<5;
				b = pixel;
				r*=255/31;
				g*=255/31;
				b*=255/31;
				tmp_inc[0] = b;
				tmp_inc[1] = g;
				tmp_inc[2] = r;
				tmp_inc += 3;
			}
		}

		if(compress(compmem, &compmemsize, tmp_buffer, height * (width * 3 + 1))!=Z_OK)
		{
			if(tmp_buffer) free(tmp_buffer);
			goto PNGerr;
		}
		if(tmp_buffer) free(tmp_buffer);
		if(!WritePNGChunk(pp,compmemsize,"IDAT",compmem))
			goto PNGerr;
	}
	if(!WritePNGChunk(pp,0,"IEND",0))
		goto PNGerr;

	free(compmem);
	fclose(pp);

	return 1;

PNGerr:
	if(compmem)
		free(compmem);
	if(pp)
		fclose(pp);
	return(0);
}

typedef struct
{
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
	u32 cmptype;
	u32 imgsize;
	s32 hppm;
	s32 vppm;
	u32 numcol;
	u32 numimpcol;
} bmpimgheader_struct;

#include "PACKED.h"
typedef struct
{
	u16 id __PACKED;
	u32 size __PACKED;
	u16 reserved1 __PACKED;
	u16 reserved2 __PACKED;
	u32 imgoffset __PACKED;
} bmpfileheader_struct;
#include "PACKED_END.h"

int NDS_WriteBMP(const char *filename, u8 *data)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	int i,j;
	u16 * bmp = (u16 *)data;
	size_t elems_written = 0;

	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = 256;
	imageheader.height = 192*2;
	imageheader.planes = 1;
	imageheader.bpp = 24;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 3;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(j=0;j<192*2;j++)
	{
		for(i=0;i<256;i++)
		{
			u8 r,g,b;
			u16 pixel = bmp[(192*2-j-1)*256+i];
			r = pixel>>10;
			pixel-=r<<10;
			g = pixel>>5;
			pixel-=g<<5;
			b = (u8)pixel;
			r*=255/31;
			g*=255/31;
			b*=255/31;
			elems_written += fwrite(&r, 1, sizeof(u8), file); 
			elems_written += fwrite(&g, 1, sizeof(u8), file); 
			elems_written += fwrite(&b, 1, sizeof(u8), file);
		}
	}
	fclose(file);

	return 1;
}

int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	size_t elems_written = 0;
	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = width;
	imageheader.height = height;
	imageheader.planes = 1;
	imageheader.bpp = 32;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 4;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(int i=0;i<height;i++)
		for(int x=0;x<width;x++)
		{
			u8* pixel = (u8*)buf + (height-i-1)*width*4;
			pixel += (x*4);
			elems_written += fwrite(pixel+2,1,1,file);
			elems_written += fwrite(pixel+1,1,1,file);
			elems_written += fwrite(pixel+0,1,1,file);
			elems_written += fwrite(pixel+3,1,1,file);
		}
	fclose(file);

	return 1;
}

// ===============================================================================
// Message dialogs
// ===============================================================================
#define MSG_PRINT { \
	va_list args; \
	va_start (args, fmt); \
	vprintf (fmt, args); \
	va_end (args); \
}
void msgFakeInfo(const char *fmt, ...)
{
	MSG_PRINT;
}

bool msgFakeConfirm(const char *fmt, ...)
{
	MSG_PRINT;
	return true;
}

void msgFakeError(const char *fmt, ...)
{
	MSG_PRINT;
}

void msgFakeWarn(const char *fmt, ...)
{
	MSG_PRINT;
}

msgBoxInterface msgBoxFake = {
	msgFakeInfo,
	msgFakeConfirm,
	msgFakeError,
	msgFakeWarn,
};

msgBoxInterface *msgbox = &msgBoxFake;
