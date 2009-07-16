struct SSurface {
	unsigned char *Surface;

	unsigned int Pitch;
	unsigned int Width, Height;
};

void RenderHQ2X (SSurface Src, SSurface Dst);
void Render2xSaI (SSurface Src, SSurface Dst);
void RenderSuper2xSaI (SSurface Src, SSurface Dst);
void RenderSuperEagle (SSurface Src, SSurface Dst);
void RenderScanline( SSurface Src, SSurface Dst);
void RenderBilinear( SSurface Src, SSurface Dst);