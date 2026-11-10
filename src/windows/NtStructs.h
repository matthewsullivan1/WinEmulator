#pragma once 

#include "common/Types.h"
#include "windows/WinTypes.h"

using emu::ptr_t;
using win::WORD;

namespace win {

	struct LIST_ENTRY {
		ptr_t Flink;
		ptr_t Blink;
	};

	struct UNICODE_STRING {
		WORD Length;
		WORD MaximumLength;
		uint32_t Padding;
		ptr_t Buffer;
	};

}