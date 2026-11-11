// Process.h
#pragma once

#include "memory/MemoryManager.h"
#include "runtime/ModuleManager.h"
#include "memory/GuestMemory.h"
#include "loader/ImageContext.h"

namespace emu {

	class Process {
	public:
		Process() : memoryManager_(memory_) {}

		MemoryManager& memory() {
			return memoryManager_;
		}

		ModuleManager& modules() {
			return moduleManager_;
		}

		const ModuleManager& modules() const {
			return moduleManager_;
		}

		MemoryStatus initializePeb();
		MemoryStatus linkModulesList();

	private:
		GuestMemory memory_;
		MemoryManager memoryManager_;
		ModuleManager moduleManager_;

		uint64_t pebAddress_ = 0;
		uint64_t tebAddress_ = 0;
	};
}
