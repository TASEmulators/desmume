/**************************************************************************\
*
* Copyright (c) 1998-2001, Microsoft Corp.  All Rights Reserved.
*
* Module Name:
*
*   GdiplusColorMatrix.h
*
* Abstract:
*
*  GDI+ Color Matrix object, used with Graphics.DrawImage
*
\**************************************************************************/

#ifndef _GDIPLUSCOLORMATRIX_H
#define _GDIPLUSCOLORMATRIX_H

//----------------------------------------------------------------------------
// Color matrix
//----------------------------------------------------------------------------

struct ColorMatrix
{
    REAL m[5][5];
};

//----------------------------------------------------------------------------
// Color Matrix flags
//----------------------------------------------------------------------------

enum ColorMatrixFlags
{
    ColorMatrixFlagsDefault   = 0,
    ColorMatrixFlagsSkipGrays = 1,
    ColorMatrixFlagsAltGray   = 2
};

//----------------------------------------------------------------------------
// Color Adjust Type
//----------------------------------------------------------------------------

enum ColorAdjustType
{
    ColorAdjustTypeDefault,
    ColorAdjustTypeBitmap,
    ColorAdjustTypeBrush,
    ColorAdjustTypePen,
    ColorAdjustTypeText,
    ColorAdjustTypeCount,
    ColorAdjustTypeAny      // Reserved
};

//----------------------------------------------------------------------------
// Color Map
//----------------------------------------------------------------------------

struct ColorMap
{
    Color oldColor;
    Color newColor;
};

#endif
