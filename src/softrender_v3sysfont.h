#include "softrender.h"

#define xx 1
#define zz 0

namespace softrender {

	class v3sysfont {
	public:
		static int height() { return 7; }
		static int width(char c) { return charByte(c,0); }
		static int pixel(char c, int x, int y) { return charByte(c,width(c)*y+x+1); }
		static bool valid(char c) { return c>=32; }
		static bool haveContour() { return false; }

		static char charByte(byte c, int i) {

			if (c < 32 || c > 128)
				c = 2;
			else 
				c -= 32;

			static const char sbA[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char ssA[]=
			   {5,
				zz,zz,zz,zz,zz,
				zz,zz,zz,zz,zz,
				zz,xx,xx,zz,zz,
				xx,zz,zz,xx,zz,
				xx,zz,zz,xx,zz,
				xx,zz,zz,xx,zz,
				zz,xx,xx,zz,xx};

			static const char sbB[]=
			   {4,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz};

			static const char ssB[]=
			   {4,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz};

			static const char sbC[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char ssC[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				zz,xx,xx,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				zz,xx,xx};

			static const char sbD[]=
			   {4,
				xx,xx,zz,zz,
				xx,zz,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,xx,zz,
				xx,xx,zz,zz};


			static const char ssD[]=
			   {4,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,xx,xx,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,xx};

			static const char sbE[]=
			   {4,
				xx,xx,xx,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,xx};

			static const char ssE[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,xx,xx,zz,
				xx,zz,zz,zz,
				zz,xx,xx,zz};

			static const char sbF[]=
			   {4,
				xx,xx,xx,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz};

			static const char ssF[]=
			   {4,
				zz,zz,xx,zz,
				zz,xx,zz,xx,
				zz,xx,zz,zz,
				xx,xx,xx,zz,
				zz,xx,zz,zz,
				zz,xx,zz,zz,
				zz,xx,zz,zz};

			static const char sbG[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,xx,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char ssG[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				zz,xx,xx,zz,
				xx,zz,zz,zz,
				xx,zz,xx,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};


			static const char sbH[]=
			   {4,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char ssH[]=
			   {4,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char sbI[]=
			   {3,
				xx,xx,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				xx,xx,xx};

			static const char ssI[]=
			   {1,
				zz,
				xx,
				zz,
				xx,
				xx,
				xx,
				xx};

			static const char sbJ[]=
			   {4,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char ssJ[]=
			   {3,
				zz,zz,xx,
				zz,zz,zz,
				zz,zz,xx,
				zz,zz,xx,
				zz,zz,xx,
				xx,zz,xx,
				zz,xx,zz};

			static const char sbK[]=
			   {4,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,xx,zz,
				xx,xx,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char ssK[]=
			   {3,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,xx,
				xx,zz,xx,
				xx,xx,zz,
				xx,zz,xx,
				xx,zz,xx};

			static const char sbL[]=
			   {3,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,xx,xx};

			static const char ssL[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,zz,zz,
				xx,xx,xx};

			static const char sbM[]=
			   {5,
				xx,zz,zz,zz,xx,
				xx,xx,zz,xx,xx,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx};

			static const char ssM[]=
			   {5,
				zz,zz,zz,zz,zz,
				zz,zz,zz,zz,zz,
				zz,xx,zz,xx,zz,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx};

			static const char sbN[]=
			   {4,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,zz,xx,
				xx,zz,xx,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char ssN[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				xx,zz,xx,zz,
				xx,xx,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char sbO[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char ssO[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char sbP[]=
			   {4,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz};

			static const char ssP[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz,
				xx,zz,zz,zz};

			static const char sbQ[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,xx,zz,
				zz,xx,zz,xx};

			static const char ssQ[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,xx,zz,
				zz,xx,zz,xx};

			static const char sbR[]=
			   {4,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				xx,zz,zz,xx};

			static const char ssR[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,zz,zz,zz};

			static const char sbS[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,zz,
				zz,xx,xx,zz,
				zz,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz,};

			static const char ssS[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				zz,xx,xx,
				xx,zz,zz,
				zz,xx,zz,
				zz,zz,xx,
				xx,xx,zz};

			static const char sbT[]=
			   {3,
				xx,xx,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz};

			static const char ssT[]=
			   {3,
				zz,xx,zz,
				zz,xx,zz,
				xx,xx,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,xx};

			static const char sbU[]=
			   {3,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,xx,xx};

			static const char ssU[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,xx,xx};

			static const char sbV[]=
			   {3,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				zz,xx,zz};

			static const char ssV[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				zz,xx,zz};

			static const char sbW[]=
			   {5,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx,
				xx,xx,zz,xx,xx,
				xx,zz,zz,zz,xx};

			static const char ssW[]=
			   {5,
				zz,zz,zz,zz,zz,
				zz,zz,zz,zz,zz,
				xx,zz,zz,zz,xx,
				xx,zz,xx,zz,xx,
				xx,zz,xx,zz,xx,
				xx,xx,zz,xx,xx,
				xx,zz,zz,zz,xx};

			static const char sbX[]=
			   {5,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx,
				zz,xx,zz,xx,zz,
				zz,zz,xx,zz,zz,
				zz,xx,zz,xx,zz,
				xx,zz,zz,zz,xx,
				xx,zz,zz,zz,xx};

			static const char ssX[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				xx,zz,xx,
				xx,zz,xx,
				zz,xx,zz,
				xx,zz,xx,
				xx,zz,xx};

			static const char sbY[]=
			   {3,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz};

			static const char ssY[]=
			   {3,
				zz,zz,zz,
				zz,zz,zz,
				xx,zz,xx,
				xx,zz,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz};

			static const char sbZ[]=
			   {5,
				xx,xx,xx,xx,xx,
				zz,zz,zz,zz,xx,
				zz,zz,zz,xx,zz,
				zz,zz,xx,zz,zz,
				zz,xx,zz,zz,zz,
				xx,zz,zz,zz,zz,
				xx,xx,xx,xx,xx};

			static const char ssZ[]=
			   {4,
				zz,zz,zz,zz,
				zz,zz,zz,zz,
				xx,xx,xx,xx,
				zz,zz,zz,xx,
				zz,zz,xx,zz,
				zz,xx,zz,zz,
				xx,xx,xx,xx};

			static const char s1[]=
			   {3,
				zz,xx,zz,
				xx,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				xx,xx,xx};

			static const char s2[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,zz,xx,zz,
				zz,xx,zz,zz,
				xx,xx,xx,xx};

			static const char s3[]=
			   {4,
				xx,xx,xx,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				zz,xx,xx,xx,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				xx,xx,xx,xx};

			static const char s4[]=
			   {4,
				xx,zz,xx,zz,
				xx,zz,xx,zz,
				xx,zz,xx,zz,
				xx,xx,xx,xx,
				zz,zz,xx,zz,
				zz,zz,xx,zz,
				zz,zz,xx,zz};

			static const char s5[]=
			   {4,
				xx,xx,xx,xx,
				xx,zz,zz,zz,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				zz,zz,zz,xx,
				zz,zz,zz,xx,
				xx,xx,xx,zz};

			static const char s6[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,zz,
				xx,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char s7[]=
			   {3,
				xx,xx,xx,
				zz,zz,xx,
				zz,zz,xx,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz,
				zz,xx,zz};

			static const char s8[]=
			   {4,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz,
				xx,zz,zz,xx,
				xx,zz,zz,xx,
				zz,xx,xx,zz};

			static const char s9[]=
			   {3,
				xx,xx,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,xx,xx,
				zz,zz,xx,
				zz,zz,xx,
				xx,xx,xx};

			static const char s0[]=
			   {3,
				xx,xx,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,zz,xx,
				xx,xx,xx};

			static const char sQuote[]={3,
						   xx,zz,xx,
						   xx,zz,xx,
						   zz,zz,zz,
						   zz,zz,zz,
						   zz,zz,zz,
						   zz,zz,zz,
						   zz,zz,zz};

			static const char sYow[]={3,
					   zz,xx,zz,
					   xx,xx,xx,
					   xx,xx,xx,
					   xx,xx,xx,
					   zz,xx,zz,
					   zz,zz,zz,
					   zz,xx,zz};

			static const char sQuotes[]={1,
							xx,
							xx,
							zz,
							zz,
							zz,
							zz,
							zz};


			static const char sComma[]={2,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,xx,
						   xx,zz};

			static const char sPeriod[]={1,
							zz,
							zz,
							zz,
							zz,
							zz,
							zz,
							xx};

			static const char sMinus[]={2,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   xx,xx,
						   zz,zz,
						   zz,zz,
						   zz,zz};

			static const char sQuest[]={3,
						   xx,xx,xx,
						   zz,zz,xx,
						   zz,zz,xx,
						   zz,zz,xx,
						   zz,xx,xx,
						   zz,zz,zz,
						   zz,xx,zz};

			static const char sColon[]={1,
						   zz,
						   zz,
						   xx,
						   zz,
						   xx,
						   zz,
						   zz};

			static const char sch[]={3,
						zz,xx,zz,
						xx,xx,xx,
						xx,xx,xx,
						xx,xx,xx,
						zz,xx,zz};

			static const char usc[]={2,
						zz,zz,
						zz,zz,
						zz,zz,
						zz,zz,
						xx,xx};

			static const char star[]={5,
						 zz,zz,zz,zz,zz,
						 zz,zz,zz,zz,zz,
						 xx,xx,xx,xx,xx,
						 xx,xx,xx,xx,xx,
						 zz,zz,zz,zz,zz,
						 zz,zz,zz,zz,zz,
						 zz,zz,zz,zz,zz};

			static const char ss[]={2,
					   xx,xx,
					   xx,xx,
					   xx,xx,
					   xx,xx,
					   xx,xx,
					   xx,xx,
					   xx,xx};

			static const char sra[]={3,
						zz,zz,zz,
						xx,zz,zz,
						xx,xx,zz,
						xx,xx,xx,
						xx,xx,zz,
						xx,zz,zz,
						zz,zz,zz};

			static const char slParen[]={2,
							zz,xx,
							xx,zz,
							xx,zz,
							xx,zz,
							xx,zz,
							xx,zz,
							zz,xx};

			static const char srParen[]={2,
							xx,zz,
							zz,xx,
							zz,xx,
							zz,xx,
							zz,xx,
							zz,xx,
							xx,zz};

			static const char ssemic[]={2,
						   zz,xx,
						   zz,zz,
						   zz,xx,
						   zz,xx,
						   zz,xx,
						   zz,xx,
						   xx,zz};

			static const char sSlash[]={3,
						   zz,zz,zz,
						   zz,zz,xx,
						   zz,zz,xx,
						   zz,xx,zz,
						   zz,xx,zz,
						   xx,zz,zz,
						   xx,zz,zz};

			static const char sbSlash[]={3,
						   zz,zz,zz,
						   xx,zz,zz,
						   xx,zz,zz,
						   zz,xx,zz,
						   zz,xx,zz,
						   zz,zz,xx,
						   zz,zz,xx};

			static const char sBlock[]={3,
							zz,zz,zz,
							zz,zz,zz,
							xx,xx,xx,
							xx,xx,xx,
							xx,xx,xx,
							zz,zz,zz,
							zz,zz,zz};

			static const char sBlank[]={2,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz,
						   zz,zz};

			static const char sAT[]=
			{ 5,
			   zz,xx,xx,xx,zz,
			   xx,zz,zz,zz,xx,
			   xx,zz,xx,xx,xx,
			   xx,zz,xx,zz,xx,
			   xx,zz,xx,xx,xx,
			   xx,zz,zz,zz,zz,
			   zz,xx,xx,xx,zz};

			static const char sNum[]=
			{ 5,
			  zz,zz,zz,zz,zz,
			  zz,xx,zz,xx,zz,
			  xx,xx,xx,xx,xx,
			  zz,xx,zz,xx,zz,
			  xx,xx,xx,xx,xx,
			  zz,xx,zz,xx,zz,
			  zz,zz,zz,zz,zz};

			static const char sBuck[]=
			{5,
				zz,zz,zz,zz,zz,
				zz,zz,zz,zz,zz,
				zz,zz,xx,zz,zz,
				zz,xx,xx,xx,zz,
				xx,xx,xx,xx,xx,
				zz,zz,zz,zz,zz,
				zz,zz,zz,zz,zz};

			static const char sPercnt[]=
			{
				5,
				zz,zz,zz,zz,zz,
				xx,zz,zz,zz,xx,
				zz,zz,zz,xx,zz,
				zz,zz,xx,zz,zz,
				zz,xx,zz,zz,zz,
				xx,zz,zz,zz,xx,
				zz,zz,zz,zz,zz};

			static const char sCarot[]=
			{ 3,
			  zz,xx,zz,
			  xx,zz,xx,
			  zz,zz,zz,
			  zz,zz,zz,
			  zz,zz,zz,
			  zz,zz,zz,
			  zz,zz,zz};

			static const char sCopy[]=
			{ 7,
				zz,xx,xx,xx,xx,xx,zz,
				xx,zz,zz,zz,zz,zz,xx,
				xx,zz,zz,xx,xx,zz,xx,
				xx,zz,xx,zz,zz,zz,xx,
				xx,zz,zz,xx,xx,zz,xx,
				xx,zz,zz,zz,zz,zz,xx,
				zz,xx,xx,xx,xx,xx,zz};

			static const char sPtr[]=
			{ 5,
			  xx,zz,zz,zz,zz,
			  xx,xx,zz,zz,zz,
			  xx,xx,xx,zz,zz,
			  xx,xx,xx,xx,zz,
			  xx,xx,xx,xx,xx,
			  zz,zz,xx,zz,zz,
			  zz,zz,zz,xx,zz};

			static const char check[]=
			{ 7,
			  xx,zz,zz,zz,zz,zz,xx,
			  zz,xx,zz,zz,zz,xx,zz,
			  zz,zz,xx,zz,xx,zz,zz,
			  zz,zz,zz,xx,zz,zz,zz,
			  zz,zz,xx,zz,xx,zz,zz,
			  zz,xx,zz,zz,zz,xx,zz,
			  xx,zz,zz,zz,zz,zz,xx };

			static const char target[]=
			{ 7,
			  zz,zz,zz,xx,zz,zz,zz,
			  zz,zz,xx,zz,xx,zz,zz,
			  zz,xx,zz,zz,zz,xx,zz,
			  xx,zz,zz,xx,zz,zz,xx,
			  zz,xx,zz,zz,zz,xx,zz,
			  zz,zz,xx,zz,xx,zz,zz,
			  zz,zz,zz,xx,zz,zz,zz };

			static const char *smal_tbl[]=
			{ sBlank,
				sYow,  sQuote,    sNum,   sBuck,sPercnt, sCarot, sQuotes, slParen,
			 srParen,    star,    sPtr,  sComma, sMinus,sPeriod,  sSlash,      s0,
				  s1,      s2,      s3,      s4,     s5,     s6,      s7,      s8,
				  s9,  sColon,  ssemic,      ss,     ss,    sra,  sQuest,     sAT,
				 sbA,     sbB,     sbC,     sbD,    sbE,    sbF,     sbG,     sbH,
				 sbI,     sbJ,     sbK,     sbL,    sbM,    sbN,     sbO,     sbP,
				 sbQ,     sbR,     sbS,     sbT,    sbU,    sbV,     sbW,     sbX,
				 sbY,     sbZ,      ss, sbSlash,     ss, sCarot,     usc,     sch,
				 ssA,     ssB,     ssC,     ssD,    ssE,    ssF,     ssG,     ssH,
				 ssI,     ssJ,     ssK,     ssL,    ssM,    ssN,     ssO,     ssP,
				 ssQ,     ssR,     ssS,     ssT,    ssU,    ssV,     ssW,     ssX,
				 ssY,     ssZ,      ss,  target,  check,  sCopy,  sBlock,     ss};

			return smal_tbl[c][i];
		}
	

	};

}




#undef xx
#undef zz
