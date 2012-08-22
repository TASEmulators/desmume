#include <stdio.h>
#include "rar.hpp"

#include "unrar.h"

Archive::Archive() : Raw( this )
{
	OldFormat=false;
	Solid=false;

	CurBlockPos=0;
	NextBlockPos=0;

	memset(&NewMhd,0,sizeof(NewMhd));
	NewMhd.HeadType=MAIN_HEAD;
	NewMhd.HeadSize=SIZEOF_NEWMHD;
	HeaderCRC=0;
}

bool Archive::IsSignature(byte *D)
{
	bool Valid=false;
	if (D[0]==0x52)
#ifndef SFX_MODULE
		if (D[1]==0x45 && D[2]==0x7e && D[3]==0x5e)
		{
			OldFormat=true;
			Valid=true;
		}
		else
#endif
			if (D[1]==0x61 && D[2]==0x72 && D[3]==0x21 && D[4]==0x1a && D[5]==0x07 && D[6]==0x00)
			{
				OldFormat=false;
				Valid=true;
			}
	return(Valid);
}


unrar_err_t Archive::IsArchive()
{
	if (Read(MarkHead.Mark,SIZEOF_MARKHEAD)!=SIZEOF_MARKHEAD)
		return unrar_err_not_arc;

	if (IsSignature(MarkHead.Mark))
	{
		if (OldFormat)
			Seek(0,SEEK_SET);
	}
	else
	{
		if (SFXSize==0)
			return unrar_err_not_arc;
	}

	unrar_err_t error =
	ReadHeader();
	// (no need to seek to next)
	if ( error != unrar_ok )
		return error;

#ifndef SFX_MODULE
	if (OldFormat)
	{
		NewMhd.Flags=OldMhd.Flags & 0x3f;
		NewMhd.HeadSize=OldMhd.HeadSize;
	}
	else
#endif
	{
		if (HeaderCRC!=NewMhd.HeadCRC)
		{
			return unrar_err_corrupt;
		}
	}
	bool
	Volume=(NewMhd.Flags & MHD_VOLUME);
	Solid=(NewMhd.Flags & MHD_SOLID)!=0;
	bool
	Encrypted=(NewMhd.Flags & MHD_PASSWORD)!=0;

	// (removed decryption and volume handling)

	if ( Encrypted )
		return unrar_err_encrypted;

	if ( Volume )
		return unrar_err_segmented;

	return unrar_ok;
}

void Archive::SeekToNext()
{
	Seek(NextBlockPos,SEEK_SET);
}
