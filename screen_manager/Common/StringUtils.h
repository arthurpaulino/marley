// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include "Common.h"

#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma warning (disable:4996)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

// Useful for shaders with error messages..
std::string SCREEN_LineNumberString(const std::string &str);

// Other simple string utilities.

inline bool startsWith(const std::string &str, const std::string &what) {
	if (str.size() < what.size())
		return false;
	return str.substr(0, what.size()) == what;
}

inline bool endsWith(const std::string &str, const std::string &what) {
	if (str.size() < what.size())
		return false;
	return str.substr(str.size() - what.size()) == what;
}

// Only use on strings where you're only concerned about ASCII.
inline bool startsWithNoCase(const std::string &str, const std::string &what) {
	if (str.size() < what.size())
		return false;
	return strncasecmp(str.c_str(), what.c_str(), what.size()) == 0;
}

inline bool endsWithNoCase(const std::string &str, const std::string &what) {
	if (str.size() < what.size())
		return false;
	const size_t offset = str.size() - what.size();
	return strncasecmp(str.c_str() + offset, what.c_str(), what.size()) == 0;
}

void SCREEN_DataToHexString(const uint8_t *data, size_t size, std::string *output);
void SCREEN_DataToHexString(const char* prefix, uint32_t startAddr, const uint8_t* data, size_t size, std::string* output);

std::string SCREEN_PStringFromFormat(const char* format, ...);
std::string SCREEN_StringFromInt(int value);

std::string SCREEN_StripSpaces(const std::string &s);
std::string SCREEN_StripQuotes(const std::string &s);

void SCREEN_PSplitString(const std::string& str, const char delim, std::vector<std::string>& output);

void SCREEN_GetQuotedStrings(const std::string& str, std::vector<std::string>& output);

std::string SCREEN_ReplaceAll(std::string input, const std::string& src, const std::string& dest);

void SCREEN_SkipSpace(const char **ptr);

void SCREEN_truncate_cpy(char *dest, size_t destSize, const char *src);
template<size_t Count>
inline void SCREEN_truncate_cpy(char(&out)[Count], const char *src) {
	SCREEN_truncate_cpy(out, Count, src);
}

long SCREEN_parseHexLong(std::string s);
long SCREEN_parseLong(std::string s);
std::string SCREEN_PStringFromFormat(const char* format, ...);
// Cheap!
bool SCREEN_PCharArrayFromFormatV(char* out, int outsize, const char* format, va_list args);

template<size_t Count>
inline void CharArrayFromFormat(char (& out)[Count], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	SCREEN_PCharArrayFromFormatV(out, Count, format, args);
	va_end(args);
}

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SCREEN_PSplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension);

std::string SCREEN_GetFilenameFromPath(std::string full_path);
