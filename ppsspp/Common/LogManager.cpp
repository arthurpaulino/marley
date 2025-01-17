// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "ppsspp_config.h"

#include <algorithm>
#include <cstring>

#include "Common/Data/Encoding/Utf8.h"

#include "Common/LogManager.h"
#include "Common/ConsoleListener.h"
#include "Common/TimeUtil.h"
#include "Common/File/FileUtil.h"
#include "Common/StringUtils.h"

// Don't need to savestate this.
const char *hleCurrentThreadName = nullptr;

bool *g_bLogEnabledSetting = nullptr;

static const char level_to_char[8] = "-NEWIDV";

#if PPSSPP_PLATFORM(UWP) && defined(_DEBUG)
#define LOG_MSC_OUTPUTDEBUG true
#else
#define LOG_MSC_OUTPUTDEBUG false
#endif

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line, const char* fmt, ...) {
	if (g_bLogEnabledSetting && !(*g_bLogEnabledSetting))
		return;
	va_list args;
	va_start(args, fmt);
	LogManager *instance = LogManager::GetInstance();
	if (instance) {
		instance->Log(level, type, file, line, fmt, args);
	} else {
		// Fall back to printf if we're before the log manager has been initialized.
		vprintf(fmt, args);
		printf("\n");
	}
	va_end(args);
}

bool GenericLogEnabled(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type) {
	if (LogManager::GetInstance())
		return (*g_bLogEnabledSetting) && LogManager::GetInstance()->IsEnabled(level, type);
	return false;
}

LogManager *LogManager::logManager_ = NULL;

struct LogNameTableEntry {
	LogTypes::LOG_TYPE logType;
	const char *name;
};

static const LogNameTableEntry logTable[] = {
	{LogTypes::SYSTEM,     "SYSTEM"},
	{LogTypes::BOOT,       "BOOT"},
	{LogTypes::COMMON,     "COMMON"},
	{LogTypes::CPU,        "CPU"},
	{LogTypes::FILESYS,    "FILESYS"},
	{LogTypes::G3D,        "G3D"},
	{LogTypes::HLE,        "HLE"},
	{LogTypes::JIT,        "JIT"},
	{LogTypes::LOADER,     "LOADER"},
	{LogTypes::ME,         "ME"},  // Media Engine
	{LogTypes::MEMMAP,     "MEMMAP"},
	{LogTypes::SASMIX,     "SASMIX"},
	{LogTypes::SAVESTATE,  "SAVESTATE"},
	{LogTypes::FRAMEBUF,   "FRAMEBUF"},
	{LogTypes::AUDIO,      "AUDIO"},
	{LogTypes::IO,         "IO"},

	{LogTypes::SCEAUDIO,   "SCEAUDIO"},
	{LogTypes::SCECTRL,    "SCECTRL"},
	{LogTypes::SCEDISPLAY, "SCEDISP"},
	{LogTypes::SCEFONT,    "SCEFONT"},
	{LogTypes::SCEGE,      "SCEGE"},
	{LogTypes::SCEINTC,    "SCEINTC"},
	{LogTypes::SCEIO,      "SCEIO"},
	{LogTypes::SCEKERNEL,  "SCEKERNEL"},
	{LogTypes::SCEMODULE,  "SCEMODULE"},
	{LogTypes::SCENET,     "SCENET"},
	{LogTypes::SCERTC,     "SCERTC"},
	{LogTypes::SCESAS,     "SCESAS"},
	{LogTypes::SCEUTILITY, "SCEUTIL"},
	{LogTypes::SCEMISC,    "SCEMISC"},
};

LogManager::LogManager(bool *enabledSetting) {
	g_bLogEnabledSetting = enabledSetting;

	for (size_t i = 0; i < ARRAY_SIZE(logTable); i++) {
		_assert_msg_(i == logTable[i].logType, "Bad logtable at %i", (int)i);
		truncate_cpy(log_[logTable[i].logType].m_shortName, logTable[i].name);
		log_[logTable[i].logType].enabled = true;
#if defined(_DEBUG)
		log_[logTable[i].logType].level = LogTypes::LDEBUG;
#else
		log_[logTable[i].logType].level = LogTypes::LINFO;
#endif
	}

	// Remove file logging on small devices in Release mode.
#if PPSSPP_PLATFORM(UWP)
	if (IsDebuggerPresent())
		debuggerLog_ = new OutputDebugStringLogListener();
#else
#if !defined(MOBILE_DEVICE) || defined(_DEBUG)
	fileLog_ = new FileLogListener("");
	consoleLog_ = new PConsoleListener();
#ifdef _WIN32
	if (IsDebuggerPresent())
		debuggerLog_ = new OutputDebugStringLogListener();
#endif
#endif
	ringLog_ = new RingbufferLogListener();
#endif

#if !defined(MOBILE_DEVICE) || defined(_DEBUG)
	AddListener(fileLog_);
	AddListener(consoleLog_);
#if defined(_MSC_VER) && (defined(USING_WIN_UI) || PPSSPP_PLATFORM(UWP))
	if (IsDebuggerPresent() && debuggerLog_ && LOG_MSC_OUTPUTDEBUG)
		AddListener(debuggerLog_);
#endif
	AddListener(ringLog_);
#endif
}

LogManager::~LogManager() {
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i) {
#if !defined(MOBILE_DEVICE) || defined(_DEBUG)
		RemoveListener(fileLog_);
		RemoveListener(consoleLog_);
#if defined(_MSC_VER) && defined(USING_WIN_UI)
		RemoveListener(debuggerLog_);
#endif
#endif
	}

	if (fileLog_)
		delete fileLog_;
#if !defined(MOBILE_DEVICE) || defined(_DEBUG)
	delete consoleLog_;
	delete debuggerLog_;
#endif
	delete ringLog_;
}

void LogManager::ChangeFileLog(const char *filename) {
	if (fileLog_) {
		RemoveListener(fileLog_);
		delete fileLog_;
		fileLog_ = nullptr;
	}

	if (filename) {
		fileLog_ = new FileLogListener(filename);
		AddListener(fileLog_);
	}
}

void LogManager::SaveConfig(Section *section) {
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++) {
		section->Set((std::string(log_[i].m_shortName) + "Enabled").c_str(), log_[i].enabled);
		section->Set((std::string(log_[i].m_shortName) + "Level").c_str(), (int)log_[i].level);
	}
}

void LogManager::LoadConfig(Section *section, bool debugDefaults) {
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++) {
		bool enabled = false;
		int level = 0;
		section->Get((std::string(log_[i].m_shortName) + "Enabled").c_str(), &enabled, true);
		section->Get((std::string(log_[i].m_shortName) + "Level").c_str(), &level, debugDefaults ? (int)LogTypes::LDEBUG : (int)LogTypes::LERROR);
		log_[i].enabled = enabled;
		log_[i].level = (LogTypes::LOG_LEVELS)level;
	}
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line, const char *format, va_list args) {
	const LogChannel &log = log_[type];
	if (level > log.level || !log.enabled)
		return;

	LogMessage message;
	message.level = level;
	message.log = log.m_shortName;

#ifdef _WIN32
	static const char sep = '\\';
#else
	static const char sep = '/';
#endif
	const char *fileshort = strrchr(file, sep);
	if (fileshort != NULL) {
		do
			--fileshort;
		while (fileshort > file && *fileshort != sep);
		if (fileshort != file)
			file = fileshort + 1;
	}

	std::lock_guard<std::mutex> lk(log_lock_);
	GetTimeFormatted(message.timestamp);

	if (hleCurrentThreadName) {
		snprintf(message.header, sizeof(message.header), "%-12.12s %c[%s]: %s:%d",
			hleCurrentThreadName, level_to_char[(int)level],
			log.m_shortName,
			file, line);
	} else {
		snprintf(message.header, sizeof(message.header), "%s:%d %c[%s]:",
			file, line, level_to_char[(int)level],
			log.m_shortName);
	}

	char msgBuf[1024];
	va_list args_copy;

	va_copy(args_copy, args);
	size_t neededBytes = vsnprintf(msgBuf, sizeof(msgBuf), format, args);
	message.msg.resize(neededBytes + 1);
	if (neededBytes > sizeof(msgBuf)) {
		// Needed more space? Re-run vsnprintf.
		vsnprintf(&message.msg[0], neededBytes + 1, format, args_copy);
	} else {
		memcpy(&message.msg[0], msgBuf, neededBytes);
	}
	message.msg[neededBytes] = '\n';
	va_end(args_copy);

	std::lock_guard<std::mutex> listeners_lock(listeners_lock_);
	for (auto &iter : listeners_) {
		iter->Log(message);
	}
}

bool LogManager::IsEnabled(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type) {
	LogChannel &log = log_[type];
	if (level > log.level || !log.enabled)
		return false;
	return true;
}

void LogManager::Init(bool *enabledSetting) {
	logManager_ = new LogManager(enabledSetting);
}

void LogManager::Shutdown() {
	delete logManager_;
	logManager_ = NULL;
}

void LogManager::AddListener(LogListener *listener) {
	if (!listener)
		return;
	std::lock_guard<std::mutex> lk(listeners_lock_);
	listeners_.push_back(listener);
}

void LogManager::RemoveListener(LogListener *listener) {
	if (!listener)
		return;
	std::lock_guard<std::mutex> lk(listeners_lock_);
	auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
	if (iter != listeners_.end())
		listeners_.erase(iter);
}

FileLogListener::FileLogListener(const char *filename) {
#if defined(_WIN32) && !defined(__MINGW32__)
	m_logfile.open(ConvertUTF8ToWString(filename).c_str(), std::ios::app);
#else
	m_logfile.open(filename, std::ios::app);
#endif
	SetEnabled(true);
}

void FileLogListener::Log(const LogMessage &message) {
	if (!IsEnabled() || !IsValid())
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_logfile << message.timestamp << " " << message.header << " " << message.msg << std::flush;
}

void OutputDebugStringLogListener::Log(const LogMessage &message) {
#if _MSC_VER
	OutputDebugStringUTF8(message.msg.c_str());
#endif
}

void RingbufferLogListener::Log(const LogMessage &message) {
	if (!enabled_)
		return;
	messages_[curMessage_] = message;
	curMessage_++;
	if (curMessage_ >= MAX_LOGS)
		curMessage_ -= MAX_LOGS;
	count_++;
}

#ifdef _WIN32

void OutputDebugStringUTF8(const char *p) {
	wchar_t temp[4096];

	int len = std::min(4095, (int)strlen(p));
	int size = (int)MultiByteToWideChar(CP_UTF8, 0, p, len, NULL, 0);
	MultiByteToWideChar(CP_UTF8, 0, p, len, temp, size);
	temp[size] = 0;

	OutputDebugString(temp);
}

#else

void OutputDebugStringUTF8(const char *p) {
	INFO_LOG(SYSTEM, "%s", p);
}

#endif
