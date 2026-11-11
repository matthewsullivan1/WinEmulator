#pragma once

#include <string>


#include "ModuleManager.h"

namespace emu {

	Module* ModuleManager::findModule(const std::string& name) {

		std::string normalized = normalizeModuleName(name);
	
		auto it = modules_.find(normalized);

		if (it == modules_.end()) {
			return nullptr;
		}

		return &it->second;
	}


	Module* ModuleManager::findModuleByBase(ptr_t base) {
	
		for (auto& [name, module] : modules_) {
			if (module.baseAddress == base) {
				return &module;
			}
		}

		return nullptr;
	}

	Module* ModuleManager::addModule(const std::string& name) {
		
		std::string normalized = normalizeModuleName(name);

		auto [it, inserted] = modules_.try_emplace(
			normalized,
			Module{
				.name = normalized,
			}
			);

		if (inserted) {
			loadOrder_.push_back(&it->second);
		}

		return &it->second;
	}



	std::string ModuleManager::normalizeModuleName(std::string name) {
		std::transform(
			name.begin(),
			name.end(),
			name.begin(),
			[](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);

		return name;
	}


}