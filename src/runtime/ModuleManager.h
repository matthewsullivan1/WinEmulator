#pragma once
#include <unordered_map>

#include "Module.h"

namespace emu {
	class ModuleManager {
	public:
		
		Module* findModule(const std::string& name);
		Module* findModuleByBase(ptr_t base);

		Module* addModule(const std::string& name);

		Module* mainModule() { return mainModule_;  }
		void setMainModule(Module* module) {
			mainModule_ = module;
		}

		const std::vector<Module*>& loadOrder() const { return loadOrder_;  }

	private:
		static std::string normalizeModuleName(std::string name);

		std::unordered_map<std::string, Module> modules_;
		std::vector<Module*> loadOrder_;

		Module* mainModule_ = nullptr;
	};

}