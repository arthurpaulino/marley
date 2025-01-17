// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Core/WaveFile.h"
#include "Common/Log.h"
#include "Core/Config.h"

constexpr size_t PWaveFileWriter::BUFFER_SIZE;

PWaveFileWriter::PWaveFileWriter()
{
}

PWaveFileWriter::~PWaveFileWriter()
{
	Stop();
}

bool PWaveFileWriter::Start(const std::string& filename, unsigned int HLESampleRate)
{
	// Check if the file is already open
	if (file) {
		ERROR_LOG(SYSTEM, "The file %s was already open, the file header will not be written.", filename.c_str());
		return false;
	}

	file.Open(filename, "wb");
	if (!file) {
		ERROR_LOG(IO, "The file %s could not be opened for writing. Please check if it's already opened by another program.", filename.c_str());
		return false;
	}

	audio_size = 0;

	// -----------------
	// Write file header
	// -----------------
	Write4("RIFF");
	Write(100 * 1000 * 1000);  // write big value in case the file gets truncated
	Write4("WAVE");
	Write4("fmt ");

	Write(16);          // size of fmt block
	Write(0x00020001);  // two channels, uncompressed

	const uint32_t sample_rate = HLESampleRate;
	Write(sample_rate);
	Write(sample_rate * 2 * 2);  // two channels, 16bit

	Write(0x00100004);
	Write4("data");
	Write(100 * 1000 * 1000 - 32);

	// We are now at offset 44
	uint64_t offset = file.Tell();
	_assert_msg_(offset == 44, "Wrong offset: %lld", (long long)offset);
	return true;
}

void PWaveFileWriter::Stop()
{
	// u32 file_size = (u32)ftello(file);
	file.Seek(4, SEEK_SET);
	Write(audio_size + 36);

	file.Seek(40, SEEK_SET);
	Write(audio_size);

	file.Close();
}

void PWaveFileWriter::Write(u32 value)
{
	file.WriteArray(&value, 1);
}

void PWaveFileWriter::Write4(const char* ptr)
{
	file.WriteBytes(ptr, 4);
}

void PWaveFileWriter::AddStereoSamples(const short* sample_data, u32 count)
{
	_assert_msg_(file, "PWaveFileWriter - file not open.");
	_assert_msg_(count <= BUFFER_SIZE * 2, "PWaveFileWriter - buffer too small (count = %u).", count);

	if (skip_silence)
	{
		bool all_zero = true;

		for (uint32_t i = 0; i < count * 2; i++)
		{
			if (sample_data[i])
				all_zero = false;
		}

		if (all_zero)
			return;
	}

	file.WriteBytes(sample_data, count * 4);
	audio_size += count * 4;
}
