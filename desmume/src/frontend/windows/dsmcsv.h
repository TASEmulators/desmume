/*
	Copyright (C) 2006-2026 DeSmuME team

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

#ifndef DSMCSV_H
#define DSMCSV_H

#include <string>
#include <vector>

#include "types.h"

// The clipboard format of the DSM Editor: one CSV row per frame.
//
//   frame,R,L,D,U,T,S,B,A,Y,X,W,E,G,TouchX,TouchY,Touch,Mic,Lid,Reset
//   120,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
//
// The single letters are the mnemonics a .dsm record itself carries, so the
// text says the same thing as the file.  parse() also takes the friendly
// spellings (Right, Left, Down, Up, Select, Start, LShoulder, RShoulder,
// Stylus) for a table typed by hand; serialize() always writes the mnemonic.
//
// A user is expected to copy a block of frames, paste it into Notepad, edit it
// by hand and paste it back, so the format is deliberately boring: no quotes,
// no escapes, no commas inside a field.  The frame column is there so the text
// can be read on its own; paste ignores it.
//
// Nothing here touches Windows or the movie: parse() and serialize() are plain
// string work, which is what lets the editor stay a view over the timeline.
namespace dsmcsv
{

// Hard limits, checked before anything is allocated.  A long TAS movie is a
// few hundred thousand frames; half a million rows is already far past what
// anybody pastes by hand, and 16 M characters is the point where the clipboard
// stops being text and starts being a denial of service.
enum { MAX_ROWS = 500000, MAX_CHARS = 16 * 1024 * 1024 };

struct Row
{
	u32	 frame;		// for reading only - parse() never looks at it
	u16	 pad;		// the 13 pad bits, see MovieRecord::mnemonics
	u8	 touchX;	// screen pixels, 0..255
	u8	 touchY;	// screen pixels, 0..191
	bool touch;		// the stylus is down on this frame
	bool mic;		// MOVIECMD_MIC
	bool lid;		// MOVIECMD_LID
	bool reset;		// MOVIECMD_RESET
};

enum Result
{
	Ok,
	Empty,			// nothing but blanks
	TooLarge,		// more than MAX_CHARS
	BadHeader,		// a column is missing, unknown or appears twice
	BadFieldCount,	// a data row does not have as many fields as the header
	BadFrame,		// the frame column is not a non-negative decimal integer
	BadValue,		// a bit column is not 0, 1 or empty, or a coordinate is out of range
	NoRows,			// a header and nothing else
	TooManyRows		// more than MAX_ROWS
};

// Reads the columns into one Row per frame.  The frame column is validated and
// then thrown away, columns may be in any order, and the names are matched
// without regard to case or surrounding blanks.
//
// badLine, when given, receives the 1-based line number of the line that was
// refused, counted the way the text editor the user pasted from counts them
// (blank lines included).  It stays 0 when the text as a whole was refused.
// An error code alone would leave somebody hunting for one bad field in five
// hundred lines by hand.
Result parse(const std::wstring &text, std::vector<Row> &rows, int *badLine = 0);

// Writes the fixed header followed by one line per row, CRLF and a trailing
// newline so Notepad is happy.
Result serialize(const std::vector<Row> &rows, std::wstring &text);

} // namespace dsmcsv

#endif // DSMCSV_H
