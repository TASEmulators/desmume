/*---------------------------------------------------------------------------------

	Simple console print demo
	-- dovoto

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	u32 *ptr = (u32*)0x02400000;

	consoleDemoInit();  //setup the sub screen for printing

	printf("twltouch\n");
	swiWaitForVBlank();

	while(1) {

		DC_InvalidateRange(ptr, 0x10);
		iprintf("\x1b[10;0H%x, x = %04i, y = %04i\n", ptr[0], ptr[1], ptr[2]);

		swiWaitForVBlank();
		scanKeys();
		if (keysDown()&KEY_START) break;
	}

	return 0;
}
