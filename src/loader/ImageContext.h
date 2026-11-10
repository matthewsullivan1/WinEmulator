#pragma once

#include <string>
#include <filesystem>
#include <cstdint>

#include "Module.h"

namespace emu {

	// For internal bookkeeping while loading. Refers to actual guest memory modules
	struct ImageContext {
		Module& module;
		ptr_t preferredBase;
	};
};