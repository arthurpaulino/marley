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

#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "Core/Util/AudioFormat.h"
#include "Core/Util/AudioFormatNEON.h"

#ifdef _M_SSE
#include <emmintrin.h>
#endif

void AdjustVolumeBlockStandard(s16 *out, s16 *in, size_t size, int leftVol, int rightVol) {
#ifdef _M_SSE
	if (leftVol <= 0x7fff && -leftVol <= 0x8000 && rightVol <= 0x7fff && -rightVol <= 0x8000) {
		__m128i volume = _mm_set_epi16(leftVol, rightVol, leftVol, rightVol, leftVol, rightVol, leftVol, rightVol);
		while (size >= 16) {
			__m128i indata1 = _mm_loadu_si128((__m128i *)in);
			__m128i indata2 = _mm_loadu_si128((__m128i *)(in + 8));
			_mm_storeu_si128((__m128i *)out, _mm_mulhi_epi16(indata1, volume));
			_mm_storeu_si128((__m128i *)(out + 8), _mm_mulhi_epi16(indata2, volume));
			in += 16;
			out += 16;
			size -= 16;
		}
	}
#if 0
	// This path wraps instead of clamps in _mm_slli_epi16. Needs fixing.
	else {
		// We have to shift inside the loop to avoid the signed 16-bit multiply issue.
		int leftShift = 0;
		int leftVol16 = leftVol;
		while (leftVol16 > 0x7fff || -leftVol16 > 0x8000) {
			++leftShift;
			leftVol16 >>= 1;
		}
		int rightShift = 0;
		int rightVol16 = rightVol;
		while (rightVol16 > 0x7fff || -rightVol16 > 0x8000) {
			++rightShift;
			rightVol16 >>= 1;
		}
		__m128i volume = _mm_set_epi16(leftVol16, rightVol16, leftVol16, rightVol16, leftVol16, rightVol16, leftVol16, rightVol16);
		while (size >= 16) {
			__m128i indata1 = _mm_loadu_si128((__m128i *)in);
			__m128i indata2 = _mm_loadu_si128((__m128i *)(in + 8));
			_mm_storeu_si128((__m128i *)out, _mm_slli_epi16(_mm_mulhi_epi16(indata1, volume), leftShift));
			_mm_storeu_si128((__m128i *)(out + 8), _mm_slli_epi16(_mm_mulhi_epi16(indata2, volume), rightShift));
			in += 16;
			out += 16;
			size -= 16;
		}
	}
#endif
#endif
	if (leftVol <= 0x7fff && -leftVol <= 0x8000 && rightVol <= 0x7fff && -rightVol <= 0x8000) {
		for (size_t i = 0; i < size; i += 2) {
			out[i] = ApplySampleVolume(in[i], leftVol);
			out[i + 1] = ApplySampleVolume(in[i + 1], rightVol);
		}
	} else {
		for (size_t i = 0; i < size; i += 2) {
			out[i] = ApplySampleVolume20Bit(in[i], leftVol);
			out[i + 1] = ApplySampleVolume20Bit(in[i + 1], rightVol);
		}
	}
}

void ConvertS16ToF32(float *out, const s16 *in, size_t size) {
#ifdef _M_SSE
	const __m128i zero = _mm_setzero_si128();
	const __m128 scale = _mm_set_ps1(1.0f / 32767.0f);
	while (size >= 16) {
		__m128i indata1 = _mm_loadu_si128((__m128i *)in + 0);
		__m128i indata2 = _mm_loadu_si128((__m128i *)in + 1);
		// Now we unpack with "sign extension", by unpacking with 0xFFFF if zero is greater.
		__m128i insign1 = _mm_cmpgt_epi16(zero, indata1);
		__m128i insign2 = _mm_cmpgt_epi16(zero, indata2);
		__m128i indata1lo = _mm_unpacklo_epi16(indata1, insign1);
		__m128i indata1hi = _mm_unpackhi_epi16(indata1, insign1);
		__m128i indata2lo = _mm_unpacklo_epi16(indata2, insign2);
		__m128i indata2hi = _mm_unpackhi_epi16(indata2, insign2);

		_mm_storeu_ps(out + 0, _mm_mul_ps(_mm_cvtepi32_ps(indata1lo), scale));
		_mm_storeu_ps(out + 4, _mm_mul_ps(_mm_cvtepi32_ps(indata1hi), scale));
		_mm_storeu_ps(out + 8, _mm_mul_ps(_mm_cvtepi32_ps(indata2lo), scale));
		_mm_storeu_ps(out + 12, _mm_mul_ps(_mm_cvtepi32_ps(indata2hi), scale));

		in += 16;
		out += 16;
		size -= 16;
	}
#endif
	for (size_t i = 0; i < size; i++) {
		out[i] = in[i] * (1.0f / 32767.0f);
	}
}

#if !defined(_M_SSE) && !PPSSPP_ARCH(ARM64)
AdjustVolumeBlockFunc AdjustVolumeBlock = &AdjustVolumeBlockStandard;

// This has to be done after CPUDetect has done its magic.
void SetupAudioFormats() {
#if PPSSPP_ARCH(ARM_NEON) && !PPSSPP_ARCH(ARM64)
	if (Pcpu_info.bNEON) {
		AdjustVolumeBlock = &AdjustVolumeBlockNEON;
	}
#endif
}
#else
void SetupAudioFormats() {
}
#endif
