/**************************************************************************\
*
* Copyright (c) 2000-2001, Microsoft Corp.  All Rights Reserved.
*
* Module Name:
*
*   GdiplusImageCodec.h
*
* Abstract:
*
*   GDI+ Codec Image APIs
*
\**************************************************************************/

#ifndef _GDIPLUSIMAGECODEC_H
#define _GDIPLUSIMAGECODEC_H

//--------------------------------------------------------------------------
// Codec Management APIs
//--------------------------------------------------------------------------

inline Status 
GetImageDecodersSize(
    OUT UINT *numDecoders,
    OUT UINT *size)
{
    return DllExports::GdipGetImageDecodersSize(numDecoders, size);
}


inline Status 
GetImageDecoders(
    IN UINT numDecoders,
    IN UINT size,
    OUT ImageCodecInfo *decoders)
{
    return DllExports::GdipGetImageDecoders(numDecoders, size, decoders);
}


inline Status 
GetImageEncodersSize(
    OUT UINT *numEncoders, 
    OUT UINT *size)
{
    return DllExports::GdipGetImageEncodersSize(numEncoders, size);
}


inline Status 
GetImageEncoders(
    IN UINT numEncoders,
    IN UINT size,
    OUT ImageCodecInfo *encoders)
{
    return DllExports::GdipGetImageEncoders(numEncoders, size, encoders);
}

#endif  // _GDIPLUSIMAGECODEC_H
