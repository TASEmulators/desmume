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
// example output LogicalName:  "C:\games.zip|Sonic.smd"
// example output PhysicalName: "C:\Documents and Settings\User\Local Settings\Temp\Gens\rom7A37.smd"
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
// NULL resets this to the default (main Gens emulator window)
void SetArchiveParentHWND(void* hwnd=NULL);

// the rest of these are more internal utility functions,
// but they could be generally useful outside of that
const char* GetTempFile(const char* category=NULL, const char* extension=NULL); // creates a temp file and returns a path to it.  extension if any should include the '.'
void ReleaseTempFile(const char* filename); // deletes a particular temporary file, by filename
int ChooseItemFromArchive(ArchiveFile& archive, bool autoChooseIfOnly1=true, const char** ignoreExtensions=0, int numIgnoreExtensions=0); // gets an index to a file within an already-open archive, using the file chooser if there's more than one choice

#endif