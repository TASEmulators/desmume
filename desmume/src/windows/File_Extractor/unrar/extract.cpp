#include <stdio.h>
#include "rar.hpp"

#include "unrar.h"

#define DataIO Arc

unrar_err_t CmdExtract::ExtractCurrentFile( bool SkipSolid, bool check_compatibility_only )
{
	check( Arc.GetHeaderType() == FILE_HEAD );

	if ( Arc.NewLhd.Flags & (LHD_SPLIT_AFTER | LHD_SPLIT_BEFORE) )
		return unrar_err_segmented;

	if ( Arc.NewLhd.Flags & LHD_PASSWORD )
		return unrar_err_encrypted;
	
	if ( !check_compatibility_only )
	{
		check(   Arc.NextBlockPos-Arc.NewLhd.FullPackSize == Arc.Tell() );
		Arc.Seek(Arc.NextBlockPos-Arc.NewLhd.FullPackSize,SEEK_SET);
	}
	
	// (removed lots of command-line handling)

#ifdef SFX_MODULE
	if ((Arc.NewLhd.UnpVer!=UNP_VER && Arc.NewLhd.UnpVer!=29) &&
			Arc.NewLhd.Method!=0x30)
#else
	if (Arc.NewLhd.UnpVer<13 || Arc.NewLhd.UnpVer>UNP_VER)
#endif
	{
		if (Arc.NewLhd.UnpVer>UNP_VER)
			return unrar_err_new_algo;
		return unrar_err_old_algo;
	}
	
	if ( check_compatibility_only )
		return unrar_ok;
	
	// (removed lots of command-line/encryption/volume handling)
	
	update_first_file_pos();
	FileCount++;
	DataIO.UnpFileCRC=Arc.OldFormat ? 0 : 0xffffffff;
	// (removed decryption)
	DataIO.SetPackedSizeToRead(Arc.NewLhd.FullPackSize);
	// (removed command-line handling)
	DataIO.SetSkipUnpCRC(SkipSolid);

	if (Arc.NewLhd.Method==0x30)
		UnstoreFile(Arc.NewLhd.FullUnpSize);
	else
	{
		// Defer creation of Unpack until first extraction
		if ( !Unp )
		{
			Unp = new Unpack( &Arc );
			if ( !Unp )
				return unrar_err_memory;
			
			Unp->Init( NULL );
		}
		
		Unp->SetDestSize(Arc.NewLhd.FullUnpSize);
#ifndef SFX_MODULE
		if (Arc.NewLhd.UnpVer<=15)
			Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
		else
#endif
			Unp->DoUnpack(Arc.NewLhd.UnpVer,Arc.NewLhd.Flags & LHD_SOLID);
	}
	
	// (no need to seek to next file)
	
	if (!SkipSolid)
	{
	    if (Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC) ||
	        !Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC^0xffffffff))
	    {
	    	// CRC is correct
	    }
	    else
	    {
			return unrar_err_corrupt;
	    }
	}
	
	// (removed broken file handling)
	// (removed command-line handling)

	return unrar_ok;
}


void CmdExtract::UnstoreFile(Int64 DestUnpSize)
{
	Buffer.Alloc(Min(DestUnpSize,0x10000));
	while (1)
	{
		unsigned int Code=DataIO.UnpRead(&Buffer[0],Buffer.Size());
		if (Code==0 || (int)Code==-1)
			break;
		Code=Code<DestUnpSize ? Code:int64to32(DestUnpSize);
		DataIO.UnpWrite(&Buffer[0],Code);
		if (DestUnpSize>=0)
			 DestUnpSize-=Code;
	}
	Buffer.Reset();
}
