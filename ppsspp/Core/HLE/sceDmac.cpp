// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "Common/Serialize/Serializer.h"
#include "Common/Serialize/SerializeFuncs.h"
#include "Core/CoreTiming.h"
#include "Core/MemMapHelpers.h"
#include "Core/Reporting.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/sceDmac.h"
#include "Core/HLE/sceKernel.h"
#include "Core/HLE/FunctionWrappers.h"
#include "Core/Debugger/Breakpoints.h"
#include "GPU/GPUInterface.h"
#include "GPU/GPUState.h"

u64 dmacMemcpyDeadline;

void __DmacInit() {
	dmacMemcpyDeadline = 0;
}

void __DmacDoState(PointerWrap &p) {
	auto s = p.Section("sceDmac", 0, 1);
	if (s == 0) {
		dmacMemcpyDeadline = 0;
		return;
	}

	Do(p, dmacMemcpyDeadline);
}

static int __DmacMemcpy(u32 dst, u32 src, u32 size) {
	bool skip = false;
	if (PMemory::IsVRAMAddress(src) || PMemory::IsVRAMAddress(dst)) {
		skip = gpu->PerformMemoryCopy(dst, src, size);
	}
	if (!skip) {
		PMemory::Memcpy(dst, PMemory::GetPointer(src), size);
		currentMIPS->InvalidateICache(dst, size);
	}

	// This number seems strangely reproducible.
	if (size >= 272) {
		// Approx. 225 MiB/s or 235929600 B/s, so let's go with 236 B/us.
		int delayUs = size / 236;
		dmacMemcpyDeadline = PCoreTiming::GetTicks() + usToCycles(delayUs);
		return hleDelayResult(0, "dmac copy", delayUs);
	}
	return 0;
}

static u32 sceDmacMemcpy(u32 dst, u32 src, u32 size) {
	if (size == 0) {
		// Some games seem to do this frequently.
		DEBUG_LOG(HLE, "sceDmacMemcpy(dest=%08x, src=%08x, size=%i): invalid size", dst, src, size);
		return SCE_KERNEL_ERROR_INVALID_SIZE;
	}
	if (!PMemory::IsValidAddress(dst) || !PMemory::IsValidAddress(src)) {
		ERROR_LOG(HLE, "sceDmacMemcpy(dest=%08x, src=%08x, size=%i): invalid address", dst, src, size);
		return SCE_KERNEL_ERROR_INVALID_POINTER;
	}
	if (dst + size >= 0x80000000 || src + size >= 0x80000000 || size >= 0x80000000) {
		ERROR_LOG(HLE, "sceDmacMemcpy(dest=%08x, src=%08x, size=%i): illegal size", dst, src, size);
		return SCE_KERNEL_ERROR_PRIV_REQUIRED;
	}

	if (dmacMemcpyDeadline > PCoreTiming::GetTicks()) {
		WARN_LOG_REPORT_ONCE(overlapDmacMemcpy, HLE, "sceDmacMemcpy(dest=%08x, src=%08x, size=%d): overlapping read", dst, src, size);
		// TODO: Should block, seems like copy doesn't start until previous finishes.
		// Might matter for overlapping copies.
	} else {
		DEBUG_LOG(HLE, "sceDmacMemcpy(dest=%08x, src=%08x, size=%i)", dst, src, size);
	}

	return __DmacMemcpy(dst, src, size);
}

static u32 sceDmacTryMemcpy(u32 dst, u32 src, u32 size) {
	if (size == 0) {
		ERROR_LOG(HLE, "sceDmacTryMemcpy(dest=%08x, src=%08x, size=%i): invalid size", dst, src, size);
		return SCE_KERNEL_ERROR_INVALID_SIZE;
	}
	if (!PMemory::IsValidAddress(dst) || !PMemory::IsValidAddress(src)) {
		ERROR_LOG(HLE, "sceDmacTryMemcpy(dest=%08x, src=%08x, size=%i): invalid address", dst, src, size);
		return SCE_KERNEL_ERROR_INVALID_POINTER;
	}
	if (dst + size >= 0x80000000 || src + size >= 0x80000000 || size >= 0x80000000) {
		ERROR_LOG(HLE, "sceDmacTryMemcpy(dest=%08x, src=%08x, size=%i): illegal size", dst, src, size);
		return SCE_KERNEL_ERROR_PRIV_REQUIRED;
	}

	if (dmacMemcpyDeadline > PCoreTiming::GetTicks()) {
		DEBUG_LOG(HLE, "sceDmacTryMemcpy(dest=%08x, src=%08x, size=%i): busy", dst, src, size);
		return SCE_KERNEL_ERROR_BUSY;
	} else {
		DEBUG_LOG(HLE, "sceDmacTryMemcpy(dest=%08x, src=%08x, size=%i)", dst, src, size);
	}

	return __DmacMemcpy(dst, src, size);
}

const HLEFunction sceDmac[] = {
	{0X617F3FE6, &WrapU_UUU<sceDmacMemcpy>,          "sceDmacMemcpy",    'x', "xxx"},
	{0XD97F94D8, &WrapU_UUU<sceDmacTryMemcpy>,       "sceDmacTryMemcpy", 'x', "xxx"},
};

void Register_sceDmac() {
	RegisterModule("sceDmac", ARRAY_SIZE(sceDmac), sceDmac);
}
