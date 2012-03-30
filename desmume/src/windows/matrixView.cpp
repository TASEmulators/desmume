/*
	Copyright (C) 2007 Acid Burn
	Copyright (C) 2007-2011 DeSmuME team

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

#include "matrixView.h"
#include <commctrl.h>
#include "debug.h"
#include "resource.h"
#include "main.h"
#include "gfx3d.h"

typedef struct
{
	u32	autoup_secs;
	bool autoup;
} matrixview_struct;

matrixview_struct	*MatrixView = NULL;

void MatrixView_SetMatrix(HWND hwnd, const int* idcs, float* matrix)
{
	int		n;
	char	buffer[64];

	for(n = 0; n < 16; n++)
	{
		sprintf(buffer, "%.4f", matrix[n]);
		//sprintf(buffer, "%.8x", (int)(matrix[n]*4096));
		SetWindowText(GetDlgItem(hwnd, idcs[n]), buffer);
	}
}

void MatrixView_OnPaintPositionMatrix(HWND hwnd)
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
	HWND	hStackCombo = GetDlgItem(hwnd, IDC_MATRIX_VIEWER_COORD_COMBO);
	int		stackIndex;

	stackIndex = SendMessage(hStackCombo, CB_GETCURSEL, 0, 0) - 1;

	gfx3d_glGetMatrix(1, stackIndex, matrix);
	MatrixView_SetMatrix(hwnd, idcGroup, matrix);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintDirectionMatrix(HWND hwnd)
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
	HWND	hStackCombo = GetDlgItem(hwnd, IDC_MATRIX_VIEWER_DIR_COMBO);
	int		stackIndex;

	stackIndex = SendMessage(hStackCombo, CB_GETCURSEL, 0, 0) - 1;

	gfx3d_glGetMatrix(2, stackIndex, matrix);
	MatrixView_SetMatrix(hwnd, idcGroup, matrix);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintProjectionMatrix(HWND hwnd)
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
	MatrixView_SetMatrix(hwnd, idcGroup, mat);
}

//////////////////////////////////////////////////////////////////////////////

void MatrixView_OnPaintTextureMatrix(HWND hwnd)
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
	MatrixView_SetMatrix(hwnd, idcGroup, mat);
}

BOOL MatrixView_OnPaint( HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    hdc = BeginPaint(hwnd, &ps);
    
	MatrixView_OnPaintProjectionMatrix(hwnd);
	MatrixView_OnPaintPositionMatrix(hwnd);
	MatrixView_OnPaintDirectionMatrix(hwnd);
	MatrixView_OnPaintTextureMatrix(hwnd);

    EndPaint(hwnd, &ps);

    return TRUE;
}

BOOL CALLBACK ViewMatricesProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     switch (message)
     {
            case WM_INITDIALOG:
				{
					MatrixView = new matrixview_struct;
					memset(MatrixView, 0, sizeof(matrixview_struct));
					MatrixView->autoup_secs = 1;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, MatrixView->autoup_secs);
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
					return 1;
				}

            case WM_CLOSE:
				{
					if(MatrixView->autoup)
					{
						KillTimer(hwnd, IDT_VIEW_MATRIX);
						MatrixView->autoup = false;
					}

					delete MatrixView;
					MatrixView = NULL;
					//INFO("Close Matrix view dialog\n");
					PostQuitMessage(0);
					return 0;
				}

            case WM_PAINT:
				MatrixView_OnPaint(hwnd, wParam, lParam);
                 return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;

            case WM_COMMAND:
                 switch (LOWORD (wParam))
                 {
                        case IDOK:
							SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 1;

						case IDC_AUTO_UPDATE :
							 if(MatrixView->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_MATRIX);
                                  MatrixView->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             MatrixView->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_MATRIX, MatrixView->autoup_secs*20, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!MatrixView) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != MatrixView->autoup_secs)
								{
									MatrixView->autoup_secs = t;
									if (MatrixView->autoup)
										SetTimer(hwnd, IDT_VIEW_MATRIX, 
												MatrixView->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;

						case IDC_MATRIX_VIEWER_DIR_COMBO:
						case IDC_MATRIX_VIEWER_COORD_COMBO:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                 }
                 return 0;
     }

	return false;
}
