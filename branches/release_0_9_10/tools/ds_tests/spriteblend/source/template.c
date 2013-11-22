#include <nds.h>
#include <nds/registers_alt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "drunkenlogo.h"
#include "plain.h"

int main(void)
{
	//---------------------------------------------------------------------------------
	int i = 0;
	touchPosition touch;

	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_SPRITE);

	oamInit(&oamMain, SpriteMapping_1D_32, false);
	/*oamInit(&oamSub, SpriteMapping_1D_32, false);*/
    consoleDemoInit();
	int bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0,0);
	int bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 4,0);

	//display bg3 above bg2. when the window effect excludes bg3, will bg2 be displayed?
	bgSetPriority(bg3,0);
	bgSetPriority(bg2,1);


	dmaCopy(drunkenlogoBitmap, bgGetGfxPtr(bg3), drunkenlogoBitmapLen);
	dmaCopy(drunkenlogoPal, BG_PALETTE, 256*2);
	dmaCopy(plainBitmap, bgGetGfxPtr(bg2), plainBitmapLen);

	u16* gfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
	/*u16* gfxSub = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_256Color);*/
    REG_BLDCNT = (1<<11) | (1<<6);
    
    REG_WININ = 0x3F; //display everything, including color special effects, in window 0
    REG_WINOUT = 0x17; //display everything, excluding color special effects and BG3 outside window 0
    REG_WIN0H = (0<<8) | 128; //window is x (0..128)
    REG_WIN0V = (0<<8) | 128; //window is y (0..128)
    REG_DISPCNT |= (1<<13); //enable use of window 0

	for(i = 0; i < 16 * 16 / 2; i+=1)
	{
		gfx[i] = 0x1111;
	}

    
	SPRITE_PALETTE[1] = RGB15(31,31,31);

    u8 eva = 0xf, evb = 0xf;

	while(1)
	{

		scanKeys();
        u32 keys = keysDown();

		if(keysHeld() & KEY_TOUCH)
			touchRead(&touch);
        if(keys & KEY_LEFT && evb > 0)
            evb--;
        if(keys & KEY_RIGHT && evb < 0xf)
            evb++;
        if(keys & KEY_UP && eva > 0)
            eva--;
        if(keys & KEY_DOWN && eva < 0xf)
            eva++;

        consoleClear();
        printf("eva %x evb %x\n", eva, evb);

		oamSet(&oamMain, //main graphics engine context
			0,           //oam index (0 to 127)  
			touch.px, touch.py,   //x and y pixle location of the sprite
			0,                    //priority, lower renders last (on top)
			0,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite	
			SpriteSize_16x16,     
			SpriteColorFormat_16Color, 
			gfx,                  //pointer to the loaded graphics
			-1,                  //sprite rotation data  
			false,               //double the size when rotating?
			false,			//hide the sprite?
			false, false, //vflip, hflip
			false	//apply mosaic
			);              
		
		
        /*REG_BLDCNT = 0;*/
        REG_BLDALPHA = evb<<8|eva;

        oamMain.oamMemory[0].blendMode = OBJMODE_BLENDED;
	
		swiWaitForVBlank();

        bgUpdate();		
		oamUpdate(&oamMain);
	}

	return 0;
}
