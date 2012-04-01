/*
Copyright (C) 2009-2011 DeSmuME team

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


typedef struct {
	unsigned char *Surface;

	unsigned int Pitch;
	unsigned int Width, Height;
} SSurface;

void RenderNearest2X (SSurface Src, SSurface Dst);
void RenderLQ2X (SSurface Src, SSurface Dst);
void RenderLQ2XS (SSurface Src, SSurface Dst);
void RenderHQ2X (SSurface Src, SSurface Dst);
void RenderHQ4X (SSurface Src, SSurface Dst);
void RenderHQ2XS (SSurface Src, SSurface Dst);
void Render2xSaI (SSurface Src, SSurface Dst);
void RenderSuper2xSaI (SSurface Src, SSurface Dst);
void RenderSuperEagle (SSurface Src, SSurface Dst);
void RenderScanline( SSurface Src, SSurface Dst);
void RenderBilinear( SSurface Src, SSurface Dst);
void RenderEPX( SSurface Src, SSurface Dst);
void RenderEPXPlus( SSurface Src, SSurface Dst);
void RenderEPX_1Point5x( SSurface Src, SSurface Dst);
void RenderEPXPlus_1Point5x( SSurface Src, SSurface Dst);
void RenderNearest_1Point5x( SSurface Src, SSurface Dst);
void RenderNearestPlus_1Point5x( SSurface Src, SSurface Dst);
