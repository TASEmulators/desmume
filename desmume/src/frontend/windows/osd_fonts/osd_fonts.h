// Numeric IDs for the OSD bitmap fonts that are embedded as RCDATA in the
// executable.  Keep the numeric values stable: they are the resource names
// looked up by NativeDraw_FontData() at run time.  31000+ was chosen because
// resources.rc does not use that range.
//
// The .bin payloads are byte-for-byte copies of the AGG 2.5 embedded raster
// fonts (same names/order as fonts_list[] in aggdraw.cpp).  Keeping them as
// Win32 resources means the font tables are not compiled into any C++ source
// and the native OSD backend does not need libagg at link time.

#ifndef _OSD_FONTS_H_
#define _OSD_FONTS_H_

#define IDR_OSD_FONT_GSE4X6                 31001
#define IDR_OSD_FONT_GSE4X8                 31002
#define IDR_OSD_FONT_GSE5X7                 31003
#define IDR_OSD_FONT_GSE5X9                 31004
#define IDR_OSD_FONT_GSE6X9                 31005
#define IDR_OSD_FONT_GSE6X12                31006
#define IDR_OSD_FONT_GSE7X11                31007
#define IDR_OSD_FONT_GSE7X11_BOLD           31008
#define IDR_OSD_FONT_GSE7X15                31009
#define IDR_OSD_FONT_GSE7X15_BOLD           31010
#define IDR_OSD_FONT_GSE8X16                31011
#define IDR_OSD_FONT_GSE8X16_BOLD           31012
#define IDR_OSD_FONT_MCS11_PROP             31013
#define IDR_OSD_FONT_MCS11_PROP_CONDENSED   31014
#define IDR_OSD_FONT_MCS12_PROP             31015
#define IDR_OSD_FONT_MCS13_PROP             31016
#define IDR_OSD_FONT_MCS5X10_MONO           31017
#define IDR_OSD_FONT_MCS5X11_MONO           31018
#define IDR_OSD_FONT_MCS6X10_MONO           31019
#define IDR_OSD_FONT_MCS6X11_MONO           31020
#define IDR_OSD_FONT_MCS7X12_MONO_HIGH      31021
#define IDR_OSD_FONT_MCS7X12_MONO_LOW       31022
#define IDR_OSD_FONT_VERDANA12              31023
#define IDR_OSD_FONT_VERDANA12_BOLD         31024
#define IDR_OSD_FONT_VERDANA13              31025
#define IDR_OSD_FONT_VERDANA13_BOLD         31026
#define IDR_OSD_FONT_VERDANA14              31027
#define IDR_OSD_FONT_VERDANA14_BOLD         31028
#define IDR_OSD_FONT_VERDANA16              31029
#define IDR_OSD_FONT_VERDANA16_BOLD         31030
#define IDR_OSD_FONT_VERDANA17              31031
#define IDR_OSD_FONT_VERDANA17_BOLD         31032
#define IDR_OSD_FONT_VERDANA18              31033
#define IDR_OSD_FONT_VERDANA18_BOLD         31034

#endif // _OSD_FONTS_H_
