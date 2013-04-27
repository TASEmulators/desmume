#include "pfc.h"

namespace pfc {

t_size string::indexOf(char c,t_size base) const {
	return pfc::string_find_first(ptr(),c,base);
}
t_size string::lastIndexOf(char c,t_size base) const {
	return pfc::string_find_last(ptr(),c,base);
}
t_size string::indexOf(stringp s,t_size base) const {
	return pfc::string_find_first(ptr(),s.ptr(),base);
}
t_size string::lastIndexOf(stringp s,t_size base) const {
	return pfc::string_find_last(ptr(),s.ptr(),base);	
}
t_size string::indexOfAnyChar(stringp _s,t_size base) const {
	string s ( _s );
	const t_size len = length();
	const char* content = ptr();
	for(t_size walk = 0; walk < len; ++walk) {
		if (s.contains(content[walk])) return walk;
	}
	return ~0;
}
t_size string::lastIndexOfAnyChar(stringp _s,t_size base) const {
	string s ( _s );
	const char* content = ptr();
	for(t_size _walk = length(); _walk > 0; --_walk) {
		const t_size walk = _walk-1;
		if (s.contains(content[walk])) return walk;
	}
	return ~0;
}
bool string::startsWith(char c) const {
	return (*this)[0] == c;
}
bool string::startsWith(string s) const {
	const char * walk = ptr();
	const char * subWalk = s.ptr();
	for(;;) {
		if (*subWalk == 0) return true;
		if (*walk != *subWalk) return false;
		walk++; subWalk++;
	}
}
bool string::endsWith(char c) const {
	const t_size len = length();
	if (len == 0) return false;
	return ptr()[len-1] == c;
}
bool string::endsWith(string s) const {
	const t_size len = length(), subLen = s.length();
	if (subLen > len) return false;
	return subString(len - subLen) == s;
}

char string::firstChar() const {
	return (*this)[0];
}
char string::lastChar() const {
	const t_size len = length();
	return len > 0 ? (*this)[len-1] : (char)0;
}

string string::replace(stringp strOld, stringp strNew) const {
	t_size walk = 0;
	string ret;
	for(;;) {
		t_size next = indexOf(strOld, walk);
		if (next == ~0) {
			ret += subString(walk); break;
		}
		ret += subString(walk,next-walk) + strNew;
		walk = next + strOld.length();
	}
	return ret;
}
bool string::contains(char c) const {return indexOf(c) != ~0;}
bool string::contains(stringp s) const {return indexOf(s) != ~0;}
bool string::containsAnyChar(stringp s) const {return indexOfAnyChar(s) != ~0;}
}
