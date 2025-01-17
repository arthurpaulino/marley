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

#include <atomic>
#include <algorithm>  // min
#include <cstring>
#include <string> // System: To be able to add strings with "+"
#include <math.h>
#ifdef _WIN32
#include <process.h>
#include "CommonWindows.h"
#include <array>
#else
#include <stdarg.h>
#endif
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "ppsspp_config.h"
#include "Common/Thread/ThreadUtil.h"
#include "Common/Data/Encoding/Utf8.h"
#include "Common/Common.h"
#include "Common/ConsoleListener.h"
#include "Common/StringUtils.h"

#if defined(USING_WIN_UI)
const int LOG_PENDING_MAX = 120 * 10000;
const int LOG_LATENCY_DELAY_MS = 20;
const int LOG_SHUTDOWN_DELAY_MS = 250;
const int LOG_MAX_DISPLAY_LINES = 4000;

int PConsoleListener::refCount = 0;
HANDLE PConsoleListener::hThread = NULL;
HANDLE PConsoleListener::hTriggerEvent = NULL;
CRITICAL_SECTION PConsoleListener::criticalSection;

char *PConsoleListener::logPending = NULL;
std::atomic<uint32_t> PConsoleListener::logPendingReadPos;
std::atomic<uint32_t> PConsoleListener::logPendingWritePos;
#endif

PConsoleListener::PConsoleListener() : bHidden(true)
{
#if defined(USING_WIN_UI)
	hConsole = NULL;
	bUseColor = true;
	if (hTriggerEvent == NULL) {
		hTriggerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		InitializeCriticalSection(&criticalSection);
		logPending = new char[LOG_PENDING_MAX];
	}
	++refCount;
#elif PPSSPP_PLATFORM(ANDROID) || PPSSPP_PLATFORM(IOS) || PPSSPP_PLATFORM(UWP) || PPSSPP_PLATFORM(SWITCH)
	bUseColor = false;
#elif defined(_MSC_VER)
	bUseColor = false;
#else
	bUseColor = isatty(fileno(stdout));
#endif
}

PConsoleListener::~PConsoleListener()
{
	Close();
}

#if defined(_WIN32)
// Handle console event
bool WINAPI ConsoleHandler(DWORD msgType)
{
    if (msgType == CTRL_C_EVENT)
    {
		OutputDebugString(L"Ctrl-C!\n");
        return TRUE;
    }
    else if (msgType == CTRL_CLOSE_EVENT)
    {
        OutputDebugString(L"Close console window!\n");
		return TRUE;
    }
    /*
        Other messages:
        CTRL_BREAK_EVENT         Ctrl-Break pressed
        CTRL_LOGOFF_EVENT        User log off
        CTRL_SHUTDOWN_EVENT      System shutdown
    */
    return FALSE;
}
#endif


// 100, 100, "Dolphin Log Console"
// Open console window - width and height is the size of console window
// Name is the window title
void PConsoleListener::Init(bool AutoOpen, int Width, int Height, const char *Title)
{
#if defined(USING_WIN_UI)
	openWidth_ = Width;
	openHeight_ = Height;
	title_ = ConvertUTF8ToWString(Title);

	if (AutoOpen)
		Open();
#endif
}

void PConsoleListener::Open()
{
#if defined(USING_WIN_UI)
	if (!GetConsoleWindow())
	{
		// Open the console window and create the window handle for GetStdHandle()
		AllocConsole();
		hWnd = GetConsoleWindow();
		ShowWindow(hWnd, SW_SHOWDEFAULT);
		// disable console close button
		HMENU hMenu=GetSystemMenu(hWnd, false);
		EnableMenuItem(hMenu,SC_CLOSE,MF_GRAYED|MF_BYCOMMAND);
		// Save the window handle that AllocConsole() created
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		// Set console handler
		if(SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE)){OutputDebugString(L"Console handler is installed!\n");}
		// Set the console window title
		SetConsoleTitle(title_.c_str());
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		// Set letter space
		LetterSpace(openWidth_, LOG_MAX_DISPLAY_LINES);
		//MoveWindow(GetConsoleWindow(), 200,200, 800,800, true);
	}
	else
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	if (hTriggerEvent != NULL && hThread == NULL)
		hThread = (HANDLE)_beginthreadex(NULL, 0, &PConsoleListener::RunThread, this, 0, NULL);
#endif
}

void PConsoleListener::Show(bool bShow)
{
#if defined(USING_WIN_UI)
	if (bShow && bHidden)
	{
		if (!IsOpen())
			Open();
		ShowWindow(GetConsoleWindow(), SW_SHOW);
		bHidden = false;
	}
	else if (!bShow && !bHidden)
	{
		ShowWindow(GetConsoleWindow(), SW_HIDE);
		bHidden = true;
	}
#endif
}


void PConsoleListener::UpdateHandle()
{
#if defined(USING_WIN_UI)
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

// Close the console window and close the eventual file handle
void PConsoleListener::Close()
{
#if defined(USING_WIN_UI)

	if (--refCount <= 0)
	{
		if (hThread != NULL)
		{
			logPendingWritePos.store((u32)-1, std::memory_order_release);

			SetEvent(hTriggerEvent);
			WaitForSingleObject(hThread, LOG_SHUTDOWN_DELAY_MS);
			CloseHandle(hThread);
			hThread = NULL;
		}
		if (hTriggerEvent != NULL)
		{
			DeleteCriticalSection(&criticalSection);
			CloseHandle(hTriggerEvent);
			hTriggerEvent = NULL;
		}
		if (logPending != NULL)
		{
			delete [] logPending;
			logPending = NULL;
		}
		refCount = 0;
	}

	if (hConsole != NULL) {
		FreeConsole();
		hConsole = NULL;
	}
#else
	fflush(NULL);
#endif
}

bool PConsoleListener::IsOpen()
{
#if defined(USING_WIN_UI)
	return (hConsole != NULL);
#else
	return true;
#endif
}

/*
  LetterSpace: SetConsoleScreenBufferSize and SetConsoleWindowInfo are
	dependent on each other, that's the reason for the additional checks.  
*/
void PConsoleListener::BufferWidthHeight(int BufferWidth, int BufferHeight, int ScreenWidth, int ScreenHeight, bool BufferFirst)
{
	_dbg_assert_msg_(IsOpen(), "Don't call this before opening the console.");
#if defined(USING_WIN_UI)
	BOOL SB, SW;
	if (BufferFirst)
	{
		// Change screen buffer size
		COORD Co = {(SHORT)BufferWidth, (SHORT)BufferHeight};
		SB = SetConsoleScreenBufferSize(hConsole, Co);
		// Change the screen buffer window size
		SMALL_RECT coo = {(SHORT)0, (SHORT)0, (SHORT)ScreenWidth, (SHORT)ScreenHeight}; // top, left, right, bottom
		SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
	}
	else
	{
		// Change the screen buffer window size
		SMALL_RECT coo = {(SHORT)0, (SHORT)0, (SHORT)ScreenWidth, (SHORT)ScreenHeight}; // top, left, right, bottom
		SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
		// Change screen buffer size
		COORD Co = {(SHORT)BufferWidth, (SHORT)BufferHeight};
		SB = SetConsoleScreenBufferSize(hConsole, Co);
	}
#endif
}
void PConsoleListener::LetterSpace(int Width, int Height)
{
	_dbg_assert_msg_(IsOpen(), "Don't call this before opening the console.");
#if defined(USING_WIN_UI)
	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);

	//
	int OldBufferWidth = ConInfo.dwSize.X;
	int OldBufferHeight = ConInfo.dwSize.Y;
	int OldScreenWidth = (ConInfo.srWindow.Right - ConInfo.srWindow.Left);
	int OldScreenHeight = (ConInfo.srWindow.Bottom - ConInfo.srWindow.Top);
	//
	int NewBufferWidth = Width;
	int NewBufferHeight = Height;
	int NewScreenWidth = NewBufferWidth - 1;
	int NewScreenHeight = OldScreenHeight;

	// Width
	BufferWidthHeight(NewBufferWidth, OldBufferHeight, NewScreenWidth, OldScreenHeight, (NewBufferWidth > OldScreenWidth-1));
	// Height
	BufferWidthHeight(NewBufferWidth, NewBufferHeight, NewScreenWidth, NewScreenHeight, (NewBufferHeight > OldScreenHeight-1));

	// Resize the window too
	//MoveWindow(GetConsoleWindow(), 200,200, (Width*8 + 50),(NewScreenHeight*12 + 200), true);
#endif
}

#if defined(USING_WIN_UI)
COORD PConsoleListener::GetCoordinates(int BytesRead, int BufferWidth)
{
	COORD Ret = {0, 0};
	// Full rows
	int Step = (int)floor((float)BytesRead / (float)BufferWidth);
	Ret.Y += Step;
	// Partial row
	Ret.X = BytesRead - (BufferWidth * Step);
	return Ret;
}

unsigned int WINAPI PConsoleListener::RunThread(void *lpParam)
{
	setCurrentThreadName("Console");
	PConsoleListener *consoleLog = (PConsoleListener *)lpParam;
	consoleLog->LogWriterThread();
	return 0;
}

void PConsoleListener::LogWriterThread()
{
	char *logLocal = new char[LOG_PENDING_MAX];
	int logLocalSize = 0;

	while (true)
	{
		WaitForSingleObject(hTriggerEvent, INFINITE);
		Sleep(LOG_LATENCY_DELAY_MS);

		u32 logRemotePos = logPendingWritePos.load(std::memory_order_acquire);
		if (logRemotePos == (u32) -1)
			break;
		else if (logRemotePos == logPendingReadPos)
			continue;
		else
		{
			EnterCriticalSection(&criticalSection);
			logRemotePos = logPendingWritePos.load(std::memory_order_acquire);

			int start = 0;
			if (logRemotePos < logPendingReadPos)
			{
				const int count = LOG_PENDING_MAX - logPendingReadPos;
				memcpy(logLocal + start, logPending + logPendingReadPos, count);

				start = count;
				logPendingReadPos = 0;
			}

			const int count = logRemotePos - logPendingReadPos;
			memcpy(logLocal + start, logPending + logPendingReadPos, count);

			logPendingReadPos += count;
			LeaveCriticalSection(&criticalSection);

			// Double check.
			if (logPendingWritePos == (u32) -1)
				break;

			logLocalSize = start + count;
		}

		for (char *Text = logLocal, *End = logLocal + logLocalSize; Text < End; )
		{
			LogTypes::LOG_LEVELS Level = LogTypes::LINFO;

			char *next = (char *) memchr(Text + 1, '\033', End - Text);
			size_t Len = next - Text;
			if (next == NULL)
				Len = End - Text;

			if (Text[0] == '\033' && Text + 1 < End)
			{
				Level = (LogTypes::LOG_LEVELS) (Text[1] - '0');
				Len -= 2;
				Text += 2;
			}

			// Make sure we didn't start quitting.  This is kinda slow.
			if (logPendingWritePos == (u32) -1)
				break;

			WriteToConsole(Level, Text, Len);
			Text += Len;
		}
	}

	delete [] logLocal;
}

void PConsoleListener::SendToThread(LogTypes::LOG_LEVELS Level, const char *Text)
{
	// Oops, we're already quitting.  Just do nothing.
	if (logPendingWritePos == (u32) -1)
		return;

	int Len = (int)strlen(Text);
	if (Len > LOG_PENDING_MAX)
		Len = LOG_PENDING_MAX - 16;

	char ColorAttr[16] = "";
	int ColorLen = 0;
	if (bUseColor)
	{
		// Not ANSI, since the console doesn't support it, but ANSI-like.
		snprintf(ColorAttr, 16, "\033%d", Level);
		// For now, rather than properly support it.
		_dbg_assert_msg_(strlen(ColorAttr) == 2, "Console logging doesn't support > 9 levels.");
		ColorLen = (int)strlen(ColorAttr);
	}

	EnterCriticalSection(&criticalSection);
	u32 logWritePos = logPendingWritePos.load();
	u32 prevLogWritePos = logWritePos;
	if (logWritePos + ColorLen + Len >= LOG_PENDING_MAX)
	{
		for (int i = 0; i < ColorLen; ++i)
			logPending[(logWritePos + i) % LOG_PENDING_MAX] = ColorAttr[i];
		logWritePos += ColorLen;
		if (logWritePos >= LOG_PENDING_MAX)
			logWritePos -= LOG_PENDING_MAX;

		int start = 0;
		if (logWritePos < LOG_PENDING_MAX && logWritePos + Len >= LOG_PENDING_MAX)
		{
			const int count = LOG_PENDING_MAX - logWritePos;
			memcpy(logPending + logWritePos, Text, count);
			start = count;
			logWritePos = 0;
		}
		const int count = Len - start;
		if (count > 0)
		{
			memcpy(logPending + logWritePos, Text + start, count);
			logWritePos += count;
		}
	}
	else
	{
		memcpy(logPending + logWritePos, ColorAttr, ColorLen);
		memcpy(logPending + logWritePos + ColorLen, Text, Len);
		logWritePos += ColorLen + Len;
	}

	// Oops, we passed the read pos.
	if (prevLogWritePos < logPendingReadPos && logWritePos >= logPendingReadPos)
	{
		char *nextNewline = (char *) memchr(logPending + logWritePos, '\n', LOG_PENDING_MAX - logWritePos);
		if (nextNewline == NULL && logWritePos > 0)
			nextNewline = (char *) memchr(logPending, '\n', logWritePos);

		// Okay, have it go right after the next newline.
		if (nextNewline != NULL)
			logPendingReadPos = (u32)(nextNewline - logPending + 1);
	}

	// Double check we didn't start quitting.
	if (logPendingWritePos == (u32) -1) {
		LeaveCriticalSection(&criticalSection);
		return;
	}

	logPendingWritePos.store(logWritePos, std::memory_order::memory_order_release);
	LeaveCriticalSection(&criticalSection);

	SetEvent(hTriggerEvent);
}

void PConsoleListener::WriteToConsole(LogTypes::LOG_LEVELS Level, const char *Text, size_t Len)
{
	_dbg_assert_msg_(IsOpen(), "Don't call this before opening the console.");

	/*
	const int MAX_BYTES = 1024*10;
	char Str[MAX_BYTES];
	va_list ArgPtr;
	int Cnt;
	va_start(ArgPtr, Text);
	Cnt = vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
	va_end(ArgPtr);
	*/
	DWORD cCharsWritten;
	WORD Color;
	static wchar_t tempBuf[2048];

	switch (Level)
	{
	case NOTICE_LEVEL: // light green
		Color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case ERROR_LEVEL: // light red
		Color = FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;
	case WARNING_LEVEL: // light yellow
		Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case INFO_LEVEL: // cyan
		Color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;
	case DEBUG_LEVEL: // gray
		Color = FOREGROUND_INTENSITY;
		break;
	default: // off-white
		Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		break;
	}
	if (Len > 10)
	{
		// First 10 chars white
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		int wlen = MultiByteToWideChar(CP_UTF8, 0, Text, (int)Len, NULL, NULL);
		MultiByteToWideChar(CP_UTF8, 0, Text, (int)Len, tempBuf, wlen);
		WriteConsole(hConsole, tempBuf, 10, &cCharsWritten, NULL);
		Text += 10;
		Len -= 10;
	}
	SetConsoleTextAttribute(hConsole, Color);
	int wlen = MultiByteToWideChar(CP_UTF8, 0, Text, (int)Len, NULL, NULL);
	MultiByteToWideChar(CP_UTF8, 0, Text, (int)Len, tempBuf, wlen);
	WriteConsole(hConsole, tempBuf, (DWORD)wlen, &cCharsWritten, NULL);
}
#endif

void PConsoleListener::PixelSpace(int Left, int Top, int Width, int Height, bool Resize)
{
	_dbg_assert_msg_(IsOpen(), "Don't call this before opening the console.");
#if defined(USING_WIN_UI)
	// Check size
	if (Width < 8 || Height < 12) return;

	bool DBef = true;
	bool DAft = true;
	std::string SLog = "";

	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);
	DWORD BufferSize = ConInfo.dwSize.X * ConInfo.dwSize.Y;

	// ---------------------------------------------------------------------
	//  Save the current text
	// ------------------------
	DWORD cCharsRead = 0;
	COORD coordScreen = { 0, 0 };

	static const int MAX_BYTES = 1024 * 16;

	std::vector<std::array<wchar_t, MAX_BYTES>> Str;
	std::vector<std::array<WORD, MAX_BYTES>> Attr;

	// ReadConsoleOutputAttribute seems to have a limit at this level
	static const int ReadBufferSize = MAX_BYTES - 32;

	DWORD cAttrRead = ReadBufferSize;
	DWORD BytesRead = 0;
	while (BytesRead < BufferSize)
	{
		Str.resize(Str.size() + 1);
		if (!ReadConsoleOutputCharacter(hConsole, Str.back().data(), ReadBufferSize, coordScreen, &cCharsRead))
			SLog += PStringFromFormat("WriteConsoleOutputCharacter error");

		Attr.resize(Attr.size() + 1);
		if (!ReadConsoleOutputAttribute(hConsole, Attr.back().data(), ReadBufferSize, coordScreen, &cAttrRead))
			SLog += PStringFromFormat("WriteConsoleOutputAttribute error");

		// Break on error
		if (cAttrRead == 0) break;
		BytesRead += cAttrRead;
		coordScreen = GetCoordinates(BytesRead, ConInfo.dwSize.X);
	}
	// Letter space
	int LWidth = (int)(floor((float)Width / 8.0f) - 1.0f);
	int LHeight = (int)(floor((float)Height / 12.0f) - 1.0f);
	int LBufWidth = LWidth + 1;
	int LBufHeight = (int)floor((float)BufferSize / (float)LBufWidth);
	// Change screen buffer size
	LetterSpace(LBufWidth, LBufHeight);


	ClearScreen(true);	
	coordScreen.Y = 0;
	coordScreen.X = 0;
	DWORD cCharsWritten = 0;

	int BytesWritten = 0;
	DWORD cAttrWritten = 0;
	for (size_t i = 0; i < Attr.size(); i++)
	{
		if (!WriteConsoleOutputCharacter(hConsole, Str[i].data(), ReadBufferSize, coordScreen, &cCharsWritten))
			SLog += PStringFromFormat("WriteConsoleOutputCharacter error");
		if (!WriteConsoleOutputAttribute(hConsole, Attr[i].data(), ReadBufferSize, coordScreen, &cAttrWritten))
			SLog += PStringFromFormat("WriteConsoleOutputAttribute error");

		BytesWritten += cAttrWritten;
		coordScreen = GetCoordinates(BytesWritten, LBufWidth);
	}	

	const int OldCursor = ConInfo.dwCursorPosition.Y * ConInfo.dwSize.X + ConInfo.dwCursorPosition.X;
	COORD Coo = GetCoordinates(OldCursor, LBufWidth);
	SetConsoleCursorPosition(hConsole, Coo);

	// if (SLog.length() > 0) Log(LogTypes::LNOTICE, SLog.c_str());

	// Resize the window too
	if (Resize) MoveWindow(GetConsoleWindow(), Left,Top, (Width + 100),Height, true);
#endif
}

void PConsoleListener::Log(const LogMessage &msg) {
	char Text[2048];
	snprintf(Text, sizeof(Text), "%s %s %s", msg.timestamp, msg.header, msg.msg.c_str());
	Text[sizeof(Text) - 2] = '\n';
	Text[sizeof(Text) - 1] = '\0';

#if defined(USING_WIN_UI)
	if (hThread == NULL && IsOpen())
		WriteToConsole(msg.level, Text, strlen(Text));
	else
		SendToThread(msg.level, Text);
#else
	char ColorAttr[16] = "";
	char ResetAttr[16] = "";

	if (bUseColor)
	{
		strcpy(ResetAttr, "\033[0m");
		switch (msg.level)
		{
		case NOTICE_LEVEL: // light green
			strcpy(ColorAttr, "\033[92m");
			break;
		case ERROR_LEVEL: // light red
			strcpy(ColorAttr, "\033[91m");
			break;
		case WARNING_LEVEL: // light yellow
			strcpy(ColorAttr, "\033[93m");
			break;
		default:
			break;
		}
	}
	fprintf(stderr, "%s%s%s", ColorAttr, Text, ResetAttr);
#endif
}
// Clear console screen
void PConsoleListener::ClearScreen(bool Cursor)
{ 
	_dbg_assert_msg_(IsOpen(), "Don't call this before opening the console.");
#if defined(USING_WIN_UI)
	COORD coordScreen = { 0, 0 }; 
	DWORD cCharsWritten; 
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	DWORD dwConSize; 
	
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
	// Write space to the entire console
	FillConsoleOutputCharacter(hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten); 
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
	// Reset cursor
	if (Cursor) SetConsoleCursorPosition(hConsole, coordScreen); 
#endif
}


