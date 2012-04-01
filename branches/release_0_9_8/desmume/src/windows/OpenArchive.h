/*
	Copyright (C) 2009 DeSmuME team

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

// for retrieving files from archives and/or managing temporary files

#ifndef OPENARCHIVE_HEADER
#define OPENARCHIVE_HEADER

#include "7zip.h"

// ObtainFile()
// this is the main, high-level function for opening possibly-compressed files.
// you don't need to check whether the file is compressed beforehand,
// this function will figure that out and work correctly either way.
// it also does the work of bringing up a within-archive file selector dialog if necessary,
// which even allows navigating to a file within an archive that's within the archive.
// the output PhysicalName is the filename of an uncompressed file
// for you to load with fopen or whatever,
// unless the function fails (or is cancelled) in which case it will return false.
// example input Name:          "C:\games.zip"
// example output LogicalName:  "C:\games.zip|Sonic.nds"
// example output PhysicalName: "C:\Documents and Settings\User\Local Settings\Temp\DeSmuME\rom7A37.smd"
// assumes the three name arguments are unique character buffers with exactly 1024 bytes each
bool ObtainFile(const char* Name, char *const & LogicalName, char *const & PhysicalName, const char* category=NULL, const char** ignoreExtensions=NULL, int numIgnoreExtensions=0);

// ReleaseTempFileCategory()
// this is for deleting the temporary files that ObtainFile() can create.
// using it is optional because they will auto-delete on proper shutdown of the program,
// but it's nice to be able to clean up the files as early as possible.
// pass in the same "category" string you passed into ObtainFile(),
// and this will delete all files of that category.
// you can optionally specify one filename to not delete even if its category matches.
// note that any still-open files cannot be deleted yet and will be skipped.
void ReleaseTempFileCategory(const char* category, const char* exceptionFilename=NULL);

// sets the parent window of subsequent archive selector dialogs
// NULL resets this to the default (main emulator window)
void SetArchiveParentHWND(void* hwnd=NULL);

// the rest of these are more internal utility functions,
// but they could be generally useful outside of that
const char* GetTempFile(const char* category=NULL, const char* extension=NULL); // creates a temp file and returns a path to it.  extension if any should include the '.'
void ReleaseTempFile(const char* filename); // deletes a particular temporary file, by filename
int ChooseItemFromArchive(ArchiveFile& archive, bool autoChooseIfOnly1=true, const char** ignoreExtensions=0, int numIgnoreExtensions=0); // gets an index to a file within an already-open archive, using the file chooser if there's more than one choice

#endif