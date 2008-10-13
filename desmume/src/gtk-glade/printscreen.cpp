/* printscreen.c - this file is part of DeSmuME
 *
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "globals.h"

/////////////////////////////// PRINTSCREEN /////////////////////////////////
// TODO: improve (let choose filename, and use png)

typedef struct
{
    u32 header_size;
    s32 width;
    s32 height;
    u16 r1;
    u16 depth;
    u32 r2;
    u32 size;
    s32 r3,r4;
    u32 r5,r6;
}BmpImageHeader;

typedef struct
{
    u16 type;
    u32 size;
    u16 r1, r2;
    u32 data_offset;
}BmpFileHeader;


int WriteBMP(const char *filename,u16 *bmp){
    BmpFileHeader  fileheader;
    BmpImageHeader imageheader;
    fileheader.size = 14;
    fileheader.type = 0x4D42;
    fileheader.r1 = 0;
    fileheader.r2=0;
    fileheader.data_offset = 14+40;
    
    imageheader.header_size = 40;
    imageheader.r1 = 1;
    imageheader.depth = 24;
    imageheader.r2 = 0;
    imageheader.size = 256 * 192*2 * 3;
    imageheader.r3 = 0;
    imageheader.r4 = 0;
    imageheader.r5 = 0;
    imageheader.r6 = 0;
    imageheader.width = 256;
    imageheader.height = 192*2;
    
    FILE *fichier = fopen(filename,"wb");
    //fwrite(&fileheader, 1, 14, fichier);

    //fwrite(&imageheader, 1, 40, fichier);
    fwrite( &fileheader.type,  sizeof(fileheader.type),  1, fichier);
    fwrite( &fileheader.size,  sizeof(fileheader.size),  1, fichier);
    fwrite( &fileheader.r1,  sizeof(fileheader.r1),  1, fichier);
    fwrite( &fileheader.r2,  sizeof(fileheader.r2),  1, fichier);
    fwrite( &fileheader.data_offset,  sizeof(fileheader.data_offset),  1, fichier);
    fwrite( &imageheader.header_size, sizeof(imageheader.header_size), 1, fichier);
    fwrite( &imageheader.width, sizeof(imageheader.width), 1, fichier);
    fwrite( &imageheader.height, sizeof(imageheader.height), 1, fichier);
    fwrite( &imageheader.r1, sizeof(imageheader.r1), 1, fichier);
    fwrite( &imageheader.depth, sizeof(imageheader.depth), 1, fichier);
    fwrite( &imageheader.r2, sizeof(imageheader.r2), 1, fichier);
    fwrite( &imageheader.size, sizeof(imageheader.size), 1, fichier);
    fwrite( &imageheader.r3, sizeof(imageheader.r3), 1, fichier);
    fwrite( &imageheader.r4, sizeof(imageheader.r4), 1, fichier);
    fwrite( &imageheader.r5, sizeof(imageheader.r5), 1, fichier);
    fwrite( &imageheader.r6, sizeof(imageheader.r6), 1, fichier);
    int i,j;
    for(j=0;j<192*2;j++)for(i=0;i<256;i++){
	    u8 r,g,b;
	    u16 pixel = bmp[i+(192*2-j)*256];
	    r = pixel>>10;
	    pixel-=r<<10;
	    g = pixel>>5;
	    pixel-=g<<5;
	    b = pixel;
	    r*=255/31;
	    g*=255/31;
	    b*=255/31;
	    fwrite(&r, 1, sizeof(char), fichier); 
	    fwrite(&g, 1, sizeof(char), fichier); 
	    fwrite(&b, 1, sizeof(char), fichier); 
	}
    fclose(fichier);
return 1;
}
