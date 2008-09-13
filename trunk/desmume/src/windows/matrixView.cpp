/*  Copyright (C) 2007 Acid Burn

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <windows.h>
#include <stdio.h>
#include "matrixView.h"
#include "resource.h"
#include "matrix.h"
#include "armcpu.h"
#include "gfx3d.h"


//////////////////////////////////////////////////////////////////////////////

void MatrixView_SetMatrix(matrixview_struct* win, const int* idcs, float* matrix)
{
	int		n;
	char	buffer[64];

	for(n = 0; n < 16; n++)
	{
		sprintf(buffer, "%.4f", matrix[n]);
		//sprintf(buffer, "%.8x", (int)(matrix[n]*4096));
		SetWindowText(GetDlgItem(win->window.hwnd, idcs[n]), buffer);
	}
}

//////////////////////////////////////////////////////////////////////////////

BOOL MatrixView_OnInitDialog(HWND hwnd)
{
	int		n;
	HWND	hPosCombo = GetDlgItem(hwnd, IDC_MATRIX_VIEWER_COORD_COMBO);
	HWND	hDirCombo = GetDlgItem(hwnd, IDC_MATRIX_VIEWER_DIR_COMBO);

	// Setup position and direction matrix comboboxes with stack indices
	SendMessage(hPosCombo, CB_ADDSTRING, 0,(LPARAM)"Current");
	SendMessage(hDirCombo, CB_ADDSTRING, 0,(LPARAM)"Current");

	for(n = 0; n < 32; n++)
	{
		char buffer[4];

		sprintf(buffer, "%d", n);
		SendMessage(hPosCombo, CB_ADDSTRING, 0,(LPARAM)buffer);
		SendMessage(hDirCombo, CB_ADDSTRING, 0,(LPARAM)buffer);
	}

	SendMessage(hPosCombo, CB_SETCURSEL, 0, 0);
	SendMessage(hDirCombo, CB_SETCURSEL, 0, 0);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL MatrixView_OnClose(matrixview_struct* win)
{
	win->window.autoup = FALSE;
	CWindow_RemoveFromRefreshList(win);
	EndDialog(win->window.hwnd, 0);
	MatrixView_Deinit(win);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintPositionMatrix(matrixview_struct* win)
{
	// IDC for each matrix coefficient
	const int idcGroup[16] =
	{
		IDC_MATRIX_VIEWER_COORD_11_EDIT, IDC_MATRIX_VIEWER_COORD_12_EDIT, IDC_MATRIX_VIEWER_COORD_13_EDIT, IDC_MATRIX_VIEWER_COORD_14_EDIT,
		IDC_MATRIX_VIEWER_COORD_21_EDIT, IDC_MATRIX_VIEWER_COORD_22_EDIT, IDC_MATRIX_VIEWER_COORD_23_EDIT, IDC_MATRIX_VIEWER_COORD_24_EDIT,
		IDC_MATRIX_VIEWER_COORD_31_EDIT, IDC_MATRIX_VIEWER_COORD_32_EDIT, IDC_MATRIX_VIEWER_COORD_33_EDIT, IDC_MATRIX_VIEWER_COORD_34_EDIT,
		IDC_MATRIX_VIEWER_COORD_41_EDIT, IDC_MATRIX_VIEWER_COORD_42_EDIT, IDC_MATRIX_VIEWER_COORD_43_EDIT, IDC_MATRIX_VIEWER_COORD_44_EDIT
	};

	float	matrix[16];
	HWND	hStackCombo = GetDlgItem(win->window.hwnd, IDC_MATRIX_VIEWER_COORD_COMBO);
	int		stackIndex;

	stackIndex = SendMessage(hStackCombo, CB_GETCURSEL, 0, 0) - 1;

	gfx3d_glGetMatrix(1, stackIndex, matrix);
	MatrixView_SetMatrix(win, idcGroup, matrix);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintDirectionMatrix(matrixview_struct* win)
{
	// IDC for each matrix coefficient
	const int idcGroup[16] =
	{
		IDC_MATRIX_VIEWER_DIR_11_EDIT, IDC_MATRIX_VIEWER_DIR_12_EDIT, IDC_MATRIX_VIEWER_DIR_13_EDIT, IDC_MATRIX_VIEWER_DIR_14_EDIT,
		IDC_MATRIX_VIEWER_DIR_21_EDIT, IDC_MATRIX_VIEWER_DIR_22_EDIT, IDC_MATRIX_VIEWER_DIR_23_EDIT, IDC_MATRIX_VIEWER_DIR_24_EDIT,
		IDC_MATRIX_VIEWER_DIR_31_EDIT, IDC_MATRIX_VIEWER_DIR_32_EDIT, IDC_MATRIX_VIEWER_DIR_33_EDIT, IDC_MATRIX_VIEWER_DIR_34_EDIT,
		IDC_MATRIX_VIEWER_DIR_41_EDIT, IDC_MATRIX_VIEWER_DIR_42_EDIT, IDC_MATRIX_VIEWER_DIR_43_EDIT, IDC_MATRIX_VIEWER_DIR_44_EDIT
	};

	float	matrix[16];
	HWND	hStackCombo = GetDlgItem(win->window.hwnd, IDC_MATRIX_VIEWER_DIR_COMBO);
	int		stackIndex;

	stackIndex = SendMessage(hStackCombo, CB_GETCURSEL, 0, 0) - 1;

	gfx3d_glGetMatrix(2, stackIndex, matrix);
	MatrixView_SetMatrix(win, idcGroup, matrix);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintProjectionMatrix(matrixview_struct* win)
{
	// IDC for each matrix coefficient
	const int idcGroup[16] =
	{
		IDC_MATRIX_VIEWER_PROJ_11_EDIT, IDC_MATRIX_VIEWER_PROJ_12_EDIT, IDC_MATRIX_VIEWER_PROJ_13_EDIT, IDC_MATRIX_VIEWER_PROJ_14_EDIT,
		IDC_MATRIX_VIEWER_PROJ_21_EDIT, IDC_MATRIX_VIEWER_PROJ_22_EDIT, IDC_MATRIX_VIEWER_PROJ_23_EDIT, IDC_MATRIX_VIEWER_PROJ_24_EDIT,
		IDC_MATRIX_VIEWER_PROJ_31_EDIT, IDC_MATRIX_VIEWER_PROJ_32_EDIT, IDC_MATRIX_VIEWER_PROJ_33_EDIT, IDC_MATRIX_VIEWER_PROJ_34_EDIT,
		IDC_MATRIX_VIEWER_PROJ_41_EDIT, IDC_MATRIX_VIEWER_PROJ_42_EDIT, IDC_MATRIX_VIEWER_PROJ_43_EDIT, IDC_MATRIX_VIEWER_PROJ_44_EDIT
	};

	float mat[16];

	gfx3d_glGetMatrix(0, -1, mat);
	MatrixView_SetMatrix(win, idcGroup, mat);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintTextureMatrix(matrixview_struct* win)
{
	// IDC for each matrix coefficient
	const int idcGroup[16] =
	{
		IDC_MATRIX_VIEWER_TEX_11_EDIT, IDC_MATRIX_VIEWER_TEX_12_EDIT, IDC_MATRIX_VIEWER_TEX_13_EDIT, IDC_MATRIX_VIEWER_TEX_14_EDIT,
		IDC_MATRIX_VIEWER_TEX_21_EDIT, IDC_MATRIX_VIEWER_TEX_22_EDIT, IDC_MATRIX_VIEWER_TEX_23_EDIT, IDC_MATRIX_VIEWER_TEX_24_EDIT,
		IDC_MATRIX_VIEWER_TEX_31_EDIT, IDC_MATRIX_VIEWER_TEX_32_EDIT, IDC_MATRIX_VIEWER_TEX_33_EDIT, IDC_MATRIX_VIEWER_TEX_34_EDIT,
		IDC_MATRIX_VIEWER_TEX_41_EDIT, IDC_MATRIX_VIEWER_TEX_42_EDIT, IDC_MATRIX_VIEWER_TEX_43_EDIT, IDC_MATRIX_VIEWER_TEX_44_EDIT
	};

	float mat[16];

	gfx3d_glGetMatrix(3, -1, mat);
	MatrixView_SetMatrix(win, idcGroup, mat);
}

//////////////////////////////////////////////////////////////////////////////

BOOL MatrixView_OnPaint(matrixview_struct* win, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    hdc = BeginPaint(hwnd, &ps);
    
	MatrixView_OnPaintProjectionMatrix(win);
	MatrixView_OnPaintPositionMatrix(win);
	MatrixView_OnPaintDirectionMatrix(win);
	MatrixView_OnPaintTextureMatrix(win);

    EndPaint(hwnd, &ps);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK MatrixView_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     matrixview_struct *win = (matrixview_struct *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG:
                 return MatrixView_OnInitDialog(hwnd);

            case WM_CLOSE:
                 return MatrixView_OnClose(win);

            case WM_PAINT:
                 return MatrixView_OnPaint(win, hwnd, wParam, lParam);

            case WM_COMMAND:
                 switch (LOWORD (wParam))
                 {
                        case IDOK:
                             CWindow_RemoveFromRefreshList(win);
                             MatrixView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 1;

						case IDC_MATRIX_VIEWER_DIR_COMBO:
						case IDC_MATRIX_VIEWER_COORD_COMBO:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                 }
                 return 0;

			case WM_SYSCOMMAND:
                 switch (LOWORD (wParam))
                 {
                        case IDC_AUTO_UPDATE:
                             CWindow_ToggleAutoUpdate(win);
                             return 1;
                 }
                 return 0;
     }

     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

matrixview_struct *MatrixView_Init(HINSTANCE hInst, HWND parent)
{
   matrixview_struct *MatrixView=NULL;

   if ((MatrixView = (matrixview_struct *)malloc(sizeof(matrixview_struct))) == NULL)
      return NULL;

   if (CWindow_Init2(&MatrixView->window, hInst, parent, "Matrix viewer", IDD_MATRIX_VIEWER, MatrixView_Proc) != 0)
   {
      free(MatrixView);
      return NULL;
   }

   return MatrixView;
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_Deinit(matrixview_struct *MatrixView)
{
   if (MatrixView)
      free(MatrixView);
}

//////////////////////////////////////////////////////////////////////////////
