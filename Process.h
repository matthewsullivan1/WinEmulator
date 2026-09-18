// Process.h
#pragma once

#include "MemoryManager.h"
#include "GuestMemory.h"
#include "Module.h"

class Process {
public:
	Process() : memoryManager_(memory_) {} 

	MemoryManager& memory() {
		return memoryManager_;
	}

	const MemoryManager& memory() const {
		return memoryManager_;
	}

	Module* findModule(std::string name);

	const Module* findModule(std::string name) const;
	
	Module& addModule(Module module);

private:
	GuestMemory memory_;
	MemoryManager memoryManager_;
	std::unordered_map<std::string, Module> modules_;
};
