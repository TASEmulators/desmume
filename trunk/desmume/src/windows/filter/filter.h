/*  Copyright (C) 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

struct SSurface {
	unsigned char *Surface;

	unsigned int Pitch;
	unsigned int Width, Height;
};

void RenderNearest2X (SSurface Src, SSurface Dst);
void RenderLQ2X (SSurface Src, SSurface Dst);
void RenderLQ2XS (SSurface Src, SSurface Dst);
void RenderHQ2X (SSurface Src, SSurface Dst);
void RenderHQ2XS (SSurface Src, SSurface Dst);
void Render2xSaI (SSurface Src, SSurface Dst);
void RenderSuper2xSaI (SSurface Src, SSurface Dst);
void RenderSuperEagle (SSurface Src, SSurface Dst);
void RenderScanline( SSurface Src, SSurface Dst);
void RenderBilinear( SSurface Src, SSurface Dst);