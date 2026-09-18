// Process.cpp

#include "Process.h"

Module* Process::findModule(std::string name) {
	
	auto it = modules_.find(name);

	if (it == modules_.end()) {
		return nullptr;
	}

	return &it->second;
}

const Module* Process::findModule(std::string name) const {

	const auto it = modules_.find(name);

	if (it == modules_.end()) {
		return nullptr;
	}

	return &it->second;
}

Module& Process::addModule(Module module) {

	const auto it = modules_.find(module.name);

	if (it != modules_.end()) {
		return module;
	}

	modules_.emplace(module.name, module);

	return module;
}

