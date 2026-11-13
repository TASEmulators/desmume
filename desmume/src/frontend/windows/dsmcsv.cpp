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

// dsmcsv.cpp : the CSV the DSM Editor puts on the clipboard.
//
// Plain string work on purpose - no windows, no movie.  The editor calls
// parse()/serialize(); see dsmcsv.h for the format and the limits.

#include "dsmcsv.h"

#include <stddef.h>

namespace dsmcsv
{

namespace
{

enum FieldId
{
	F_FRAME = 0,
	F_R, F_L, F_D, F_U, F_T, F_S, F_B, F_A, F_Y, F_X, F_W, F_E, F_G,
	F_TOUCHX, F_TOUCHY, F_TOUCH, F_MIC, F_LID, F_RESET,
	F_COUNT
};

// The columns, in the order serialize() writes them and the order the grid
// shows them.  frame carries no bit: it is there for the human reading the
// text, and parse() only checks that it is a number.
//
// The names are the mnemonics a .dsm record itself carries ("RLDUTSBAYXWEG",
// see MovieRecord::mnemonics), so the text matches what the file says.  The
// second name is the friendly one a hand-written table is more likely to use;
// parse() accepts either, serialize() always writes the mnemonic.
struct Field
{
	const wchar_t *name;
	const wchar_t *alt;		// may be NULL
	FieldId		   id;
};

const Field kFields[] = {
	{ L"frame",  NULL,		   F_FRAME },
	{ L"R",		 L"Right",	   F_R },
	{ L"L",		 L"Left",	   F_L },
	{ L"D",		 L"Down",	   F_D },
	{ L"U",		 L"Up",		   F_U },
	{ L"T",		 L"Select",	   F_T },
	{ L"S",		 L"Start",	   F_S },
	{ L"B",		 NULL,		   F_B },
	{ L"A",		 NULL,		   F_A },
	{ L"Y",		 NULL,		   F_Y },
	{ L"X",		 NULL,		   F_X },
	{ L"W",		 L"LShoulder", F_W },
	{ L"E",		 L"RShoulder", F_E },
	{ L"G",		 NULL,		   F_G },
	{ L"TouchX", NULL,		   F_TOUCHX },
	{ L"TouchY", NULL,		   F_TOUCHY },
	{ L"Touch",	 L"Stylus",	   F_TOUCH },
	{ L"Mic",	 NULL,		   F_MIC },
	{ L"Lid",	 NULL,		   F_LID },
	{ L"Reset",	 NULL,		   F_RESET },
};

const int kFieldCount = int(sizeof(kFields) / sizeof(kFields[0]));

// Pad bits in the order MovieRecord stores them: R is bit 12, G is bit 0.
const u16 kPadMask[] = {
	1 << 12, 1 << 11, 1 << 10, 1 << 9, 1 << 8, 1 << 7, 1 << 6,
	1 << 5, 1 << 4, 1 << 3, 1 << 2, 1 << 1, 1 << 0,
};

bool isBlank(wchar_t c)
{
	return c == L' ' || c == L'\t';
}

bool isDigit(wchar_t c)
{
	return c >= L'0' && c <= L'9';
}

bool isLineBreak(wchar_t c)
{
	return c == L'\r' || c == L'\n';
}

std::wstring trim(const std::wstring &text)
{
	size_t first = 0;
	size_t last  = text.size();

	while (first < last && isBlank(text[first]))
		++first;
	while (last > first && isBlank(text[last - 1]))
		--last;

	return text.substr(first, last - first);
}

// ASCII case folding only: the column names are ASCII, and the clipboard may
// come from anything, so no locale is consulted anywhere in this file.
wchar_t lower(wchar_t c)
{
	if (c >= L'A' && c <= L'Z')
		return wchar_t(c - L'A' + L'a');
	return c;
}

bool sameName(const std::wstring &left, const wchar_t *right)
{
	size_t i = 0;
	for (; i < left.size(); ++i)
	{
		if (right[i] == 0 || lower(left[i]) != lower(right[i]))
			return false;
	}

	return right[i] == 0;
}

// Splits on commas.  No quoting: a field that contains a comma is a field count
// mismatch, which is what the format says should happen.
void splitFields(const std::wstring &line, std::vector<std::wstring> &fields)
{
	fields.clear();

	size_t start = 0;
	for (;;)
	{
		const size_t comma = line.find(L',', start);
		if (comma == std::wstring::npos)
		{
			fields.push_back(line.substr(start));
			return;
		}

		fields.push_back(line.substr(start, comma - start));
		start = comma + 1;
	}
}

// Non-negative decimal, no sign, no blanks, no overflow.  Hand-rolled so that
// "12abc" cannot sneak through the way %u would let it.
bool parseUnsigned(const std::wstring &field, u32 limit, u32 &value)
{
	if (field.empty())
		return false;

	u32 result = 0;
	for (size_t i = 0; i < field.size(); ++i)
	{
		if (!isDigit(field[i]))
			return false;

		const u32 digit = u32(field[i] - L'0');
		if (result > (0xFFFFFFFFu - digit) / 10)
			return false;

		result = result * 10 + digit;
	}

	if (result > limit)
		return false;

	value = result;
	return true;
}

// 0, 1 or nothing at all.
bool parseBit(const std::wstring &field, bool &set)
{
	if (field.empty())
	{
		set = false;
		return true;
	}

	if (field.size() != 1 || (field[0] != L'0' && field[0] != L'1'))
		return false;

	set = (field[0] == L'1');
	return true;
}

void appendUnsigned(std::wstring &text, u32 value)
{
	wchar_t digits[12];
	int		count = 0;

	do
	{
		digits[count++] = wchar_t(L'0' + (value % 10));
		value /= 10;
	} while (value != 0);

	while (count > 0)
		text.push_back(digits[--count]);
}

// Walks the text one line at a time, accepting CRLF, LF and a lone CR.
class LineReader
{
public:
	explicit LineReader(const std::wstring &text) : m_text(text), m_pos(0)
	{
		// A file pasted through an editor arrives with a byte order mark; it
		// is not part of the first column name, so it is skipped here rather
		// than being trimmed off every field later.
		if (!m_text.empty() && m_text[0] == 0xFEFF)
			m_pos = 1;
	}

	bool next(std::wstring &line)
	{
		if (m_pos >= m_text.size())
			return false;

		size_t end = m_pos;
		while (end < m_text.size() && !isLineBreak(m_text[end]))
			++end;

		line = m_text.substr(m_pos, end - m_pos);

		if (end < m_text.size())
		{
			if (m_text[end] == L'\r' && end + 1 < m_text.size() && m_text[end + 1] == L'\n')
				m_pos = end + 2;
			else
				m_pos = end + 1;
		}
		else
			m_pos = end;

		return true;
	}

private:
	const std::wstring &m_text;
	size_t				m_pos;
};

void storeField(Row &row, FieldId id, bool set)
{
	if (id >= F_R && id <= F_G)
	{
		if (set)
			row.pad |= kPadMask[id - F_R];
		return;
	}

	switch (id)
	{
	case F_TOUCH: row.touch = set; break;
	case F_MIC:   row.mic   = set; break;
	case F_LID:   row.lid   = set; break;
	case F_RESET: row.reset = set; break;
	default:                       break;
	}
}

bool loadField(const Row &row, FieldId id, bool &set)
{
	if (id >= F_R && id <= F_G)
	{
		set = (row.pad & kPadMask[id - F_R]) != 0;
		return true;
	}

	switch (id)
	{
	case F_TOUCH: set = row.touch; return true;
	case F_MIC:   set = row.mic;   return true;
	case F_LID:   set = row.lid;   return true;
	case F_RESET: set = row.reset; return true;
	default:                       return false;
	}
}

} // namespace

Result parse(const std::wstring &text, std::vector<Row> &rows, int *badLine)
{
	rows.clear();

	if (badLine)
		*badLine = 0;

	if (text.size() > size_t(MAX_CHARS))
		return Result::TooLarge;

	LineReader reader(text);

	std::wstring			  line;
	std::vector<std::wstring> fields;
	std::vector<FieldId>	  mapping;	// header position -> field
	int						  lineNumber = 0;

	while (reader.next(line))
	{
		++lineNumber;

		if (trim(line).empty())
			continue;	// leading, trailing and in-between blank lines

		splitFields(line, fields);

		// The first real line is the header.  Every name has to be one of the
		// twenty, exactly once each - that is what makes a paste predictable.
		if (mapping.empty())
		{
			if (int(fields.size()) != kFieldCount)
			{
				if (badLine)
					*badLine = lineNumber;
				return Result::BadHeader;
			}

			mapping.assign(fields.size(), F_COUNT);

			for (size_t i = 0; i < fields.size(); ++i)
			{
				const std::wstring name = trim(fields[i]);
				if (name.empty())
				{
					if (badLine)
						*badLine = lineNumber;
					return Result::BadHeader;
				}

				int found = -1;
				for (int c = 0; c < kFieldCount; ++c)
				{
					if (sameName(name, kFields[c].name)
					    || (kFields[c].alt != NULL && sameName(name, kFields[c].alt)))
					{
						found = c;
						break;
					}
				}

				if (found < 0)
				{
					if (badLine)
						*badLine = lineNumber;
					return Result::BadHeader;
				}

				for (size_t j = 0; j < i; ++j)
				{
					if (mapping[j] == kFields[found].id)
					{
						if (badLine)
							*badLine = lineNumber;
						return Result::BadHeader;	// the same column twice
					}
				}

				mapping[i] = kFields[found].id;
			}

			continue;
		}

		if (fields.size() != mapping.size())
		{
			if (badLine)
				*badLine = lineNumber;
			return Result::BadFieldCount;
		}

		Row row;
		row.frame  = 0;
		row.pad	   = 0;
		row.touchX = 0;
		row.touchY = 0;
		row.touch  = false;
		row.mic	   = false;
		row.lid	   = false;
		row.reset  = false;

		for (size_t i = 0; i < fields.size(); ++i)
		{
			const std::wstring field = trim(fields[i]);
			const FieldId	   id	  = mapping[i];

			if (id == F_FRAME)
			{
				u32 frame = 0;
				if (!parseUnsigned(field, 0xFFFFFFFFu, frame))
				{
					if (badLine)
						*badLine = lineNumber;
					return Result::BadFrame;
				}
				row.frame = frame;	// read for the reader's sake, never used
				continue;
			}

			if (id == F_TOUCHX || id == F_TOUCHY)
			{
				// An empty coordinate is 0: a hand-written row that only
				// toggles the stylus should not have to spell both of them out.
				u32 value = 0;
				if (!field.empty() && !parseUnsigned(field, 255, value))
				{
					if (badLine)
						*badLine = lineNumber;
					return Result::BadValue;
				}

				if (id == F_TOUCHX)
					row.touchX = u8(value);
				else
					row.touchY = u8(value);
				continue;
			}

			bool set = false;
			if (!parseBit(field, set))
			{
				if (badLine)
					*badLine = lineNumber;
				return Result::BadValue;
			}

			storeField(row, id, set);
		}

		if (rows.size() >= size_t(MAX_ROWS))
		{
			if (badLine)
				*badLine = lineNumber;
			return Result::TooManyRows;
		}

		rows.push_back(row);
	}

	if (mapping.empty())
		return Result::Empty;

	if (rows.empty())
		return Result::NoRows;

	return Result::Ok;
}

Result serialize(const std::vector<Row> &rows, std::wstring &text)
{
	text.clear();

	for (int i = 0; i < kFieldCount; ++i)
	{
		if (i)
			text.push_back(L',');
		text.append(kFields[i].name);
	}
	text.append(L"\r\n");

	for (size_t i = 0; i < rows.size(); ++i)
	{
		const Row &row = rows[i];

		appendUnsigned(text, row.frame);

		for (int c = 1; c < kFieldCount; ++c)
		{
			const FieldId id = kFields[c].id;

			text.push_back(L',');

			if (id == F_TOUCHX)
			{
				appendUnsigned(text, row.touchX);
				continue;
			}
			if (id == F_TOUCHY)
			{
				appendUnsigned(text, row.touchY);
				continue;
			}

			bool set = false;
			loadField(row, id, set);
			text.push_back(set ? L'1' : L'0');
		}

		text.append(L"\r\n");
	}

	return Result::Ok;
}

} // namespace dsmcsv
