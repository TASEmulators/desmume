#ifndef COLORCTRL_H
#define COLORCTRL_H

typedef struct
{
	HWND		hWnd;
	COLORREF	color;
} ColorCtrl;

void ColorCtrl_Register();
HWND ColorCtrl_Create(HWND hParent);
void ColorCtrl_SetColor(HWND hWnd, COLORREF color);

#endif // COLORCTRL_H
