#include "pfc.h"

namespace pfc { namespace io { namespace path {

static const string g_pathSeparators ("\\/|");

string getFileName(string path) {
	t_size split = path.lastIndexOfAnyChar(g_pathSeparators);
	if (split == ~0) return path;
	else return path.subString(split+1);
}
string getFileNameWithoutExtension(string path) {
	string fn = getFileName(path);
	t_size split = path.lastIndexOf('.');
	if (split == ~0) return fn;
	else return fn.subString(0,split);
}
string getFileExtension(string path) {
	string fn = getFileName(path);
	t_size split = fn.lastIndexOf('.');
	if (split == ~0) return "";
	else return fn.subString(split);
}
string getDirectory(string filePath) {return getParent(filePath);}

string getParent(string filePath) {
	t_size split = filePath.lastIndexOfAnyChar(g_pathSeparators);
	if (split == ~0) return "";
#ifdef _WINDOWS
	if (split > 0 && getIllegalNameChars().contains(filePath[split-1])) {
		if (split + 1 < filePath.length()) return filePath.subString(0,split+1);
		else return "";
	}
#endif
	return filePath.subString(0,split);
}
string combine(string basePath,string fileName) {
	if (basePath.length() > 0) {
		if (!isSeparator(basePath.lastChar())) {
			basePath += getDefaultSeparator();
		}
		return basePath + fileName;
	} else {
		//todo?
		return fileName;
	}
}

bool isSeparator(char c) {
	return g_pathSeparators.indexOf(c) != ~0;
}
string getSeparators() {
	return g_pathSeparators;
}

static string replaceIllegalChar(char c) {
	switch(c) {
		case '*':
			return "x";
		case '\"':
			return "\'\'";
		default:
			return "_";
	}
}
string replaceIllegalPathChars(string fn) {
	string illegal = getIllegalNameChars();
	string separators = getSeparators();
	string_formatter output;
	for(t_size walk = 0; walk < fn.length(); ++walk) {
		const char c = fn[walk];
		if (separators.contains(c)) {
			output.add_byte(getDefaultSeparator());
		} else if (string::isNonTextChar(c) || illegal.contains(c)) {
			string replacement = replaceIllegalChar(c);
			if (replacement.containsAnyChar(illegal)) /*per-OS weirdness security*/ replacement = "_";
			output << replacement.ptr();
		} else {
			output.add_byte(c);
		}
	}
	return output.toString();
}

string replaceIllegalNameChars(string fn) {
	const string illegal = getIllegalNameChars();
	string_formatter output;
	for(t_size walk = 0; walk < fn.length(); ++walk) {
		const char c = fn[walk];
		if (string::isNonTextChar(c) || illegal.contains(c)) {
			string replacement = replaceIllegalChar(c);
			if (replacement.containsAnyChar(illegal)) /*per-OS weirdness security*/ replacement = "_";
			output << replacement.ptr();
		} else {
			output.add_byte(c);
		}
	}
	return output.toString();
}

bool isInsideDirectory(pfc::string directory, pfc::string inside) {
	//not very efficient
	string walk = inside;
	for(;;) {
		walk = getParent(walk);
		if (walk == "") return false;
		if (equals(directory,walk)) return true;
	}
}
bool isDirectoryRoot(string path) {
	return getParent(path).isEmpty();
}
//OS-dependant part starts here


char getDefaultSeparator() {
#ifdef _WINDOWS
	return '\\';
#else
#error PORTME
#endif
}

static const string g_illegalNameChars(g_pathSeparators +
#ifdef _WINDOWS
									   ":<>*?\""
#else
#error PORTME
#endif
									   );

string getIllegalNameChars() {
	return g_illegalNameChars;
}

static bool isIllegalTrailingChar(char c) {
	return c == ' ' || c == '.';
}

string validateFileName(string name) {
	while(name.endsWith('?')) name = name.subString(0, name.length() - 1);
#ifdef _WINDOWS
	name = replaceIllegalNameChars(name);
	if (name.length() > 0) {
		t_size end = name.length();
		while(end > 0) {
			if (!isIllegalTrailingChar(name[end-1])) break;
			--end;
		}
		t_size begin = 0;
		while(begin < end) {
			if (!isIllegalTrailingChar(name[begin])) break;
			++begin;
		}
		if (end < name.length() || begin > 0) name = name.subString(begin,end - begin);
	}
	if (name.isEmpty()) name = "_";
	return name;
#else
	return replaceIllegalNameChars(name);
#endif
}

}}}
