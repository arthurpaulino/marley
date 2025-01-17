#include "Common/Log.h"
#include "Common/File/VFS/VFS.h"
#include "Common/File/VFS/AssetReader.h"

struct VFSEntry {
	const char *prefix;
	SCREEN_AssetReader *reader;
};

static VFSEntry entries[16];
static int num_entries = 0;

void VFSRegister(const char *prefix, SCREEN_AssetReader *reader) {
	entries[num_entries].prefix = prefix;
	entries[num_entries].reader = reader;
	printf("Registered VFS for prefix %s: %s\n", prefix, reader->toString().c_str());
	num_entries++;
}

void SCREEN_VFSShutdown() {
	for (int i = 0; i < num_entries; i++) {
		delete entries[i].reader;
	}
	num_entries = 0;
}

static bool IsLocalPath(const char *path) {
	bool isUnixLocal = path[0] == '/';

	return isUnixLocal;
}

// The returned data should be free'd with delete[].
uint8_t *SCREEN_VFSReadFile(const char *filename, size_t *size) {
	if (IsLocalPath(filename)) {
		// Local path, not VFS.
		// INFO_LOG(IO, "Not a VFS path: %s . Reading local file.", filename);
		return SCREEN_ReadLocalFile(filename, size);
	}

	int fn_len = (int)strlen(filename);
	bool fileSystemFound = false;
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(filename, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			// INFO_LOG(IO, "Prefix match: %s (%s) -> %s", entries[i].prefix, filename, filename + prefix_len);
			uint8_t *data = entries[i].reader->ReadAsset(filename + prefix_len, size);
			if (data)
				return data;
			else
				continue;
			// Else try the other registered file systems.
		}
	}
	if (!fileSystemFound) {
		printf("Missing filesystem for '%s'", filename);
	}  // Otherwise, the file was just missing. No need to log.
	return 0;
}

bool SCREEN_VFSGetFileListing(const char *path, std::vector<FileInfo> *listing, const char *filter) {
	if (IsLocalPath(path)) {
		// Local path, not VFS.
		// INFO_LOG(IO, "Not a VFS path: %s . Reading local directory.", path);
		getFilesInDir(path, listing, filter);
		return true;
	}

	int fn_len = (int)strlen(path);
	bool fileSystemFound = false;
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(path, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			if (entries[i].reader->GetFileListing(path + prefix_len, listing, filter)) {
				return true;
			}
		}
	}

	if (!fileSystemFound) {
		printf("Missing filesystem for %s", path);
	}  // Otherwise, the file was just missing. No need to log.
	return false;
}

bool SCREEN_VFSGetFileInfo(const char *path, FileInfo *info) {
	if (IsLocalPath(path)) {
		// Local path, not VFS.
		// INFO_LOG(IO, "Not a VFS path: %s . Getting local file info.", path);
		return getFileInfo(path, info);
	}

	bool fileSystemFound = false;
	int fn_len = (int)strlen(path);
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(path, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			if (entries[i].reader->GetFileInfo(path + prefix_len, info))
				return true;
			else
				continue;
		}
	}
	if (!fileSystemFound) {
		printf("Missing filesystem for %s", path);
	}  // Otherwise, the file was just missing. No need to log.
	return false;
}
