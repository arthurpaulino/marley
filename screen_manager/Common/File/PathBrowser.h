#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include <cstdlib>

#include "Common/File/DirListing.h"

// Abstraction above path that lets you navigate easily.
// "/" is a special path that means the root of the file system. On Windows,
// listing this will yield drives.
class SCREEN_PathBrowser {
public:
	SCREEN_PathBrowser() {}
	SCREEN_PathBrowser(std::string path) { SetPath(path); }
	~SCREEN_PathBrowser();

	void SetPath(const std::string &path);
	bool IsListingReady();
	bool GetListing(std::vector<FileInfo> &fileInfo, const char *filter = nullptr, bool *cancel = nullptr);
	void Navigate(const std::string &path);

	std::string GetPath() const {
		return path_;
	}
	std::string GetFriendlyPath() const {
		std::string str = GetPath();

		char *home = getenv("HOME");
		if (home != nullptr && !strncmp(str.c_str(), home, strlen(home))) {
			str = str.substr(strlen(home));
			str.insert(0, 1, '~');
		}

		return str;
	}

private:
	void HandlePath();

	std::string path_;
	std::string pendingPath_;
	std::vector<FileInfo> pendingFiles_;
	std::condition_variable pendingCond_;
	std::mutex pendingLock_;
	std::thread pendingThread_;
	bool pendingCancel_ = false;
	bool pendingStop_ = false;
	bool ready_ = false;
};

