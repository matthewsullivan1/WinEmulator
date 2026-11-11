#pragma once

#include <filesystem>
#include <unordered_map>


namespace emu {

	class ApiSetResolver {

	public:
		bool initialize(const std::filesystem::path& schemaPath);

		std::optional<std::string> resolve(const std::string& contract, const std::string& importingModule) const;

	private:
		struct HostMapping {
			std::string importingModule;
			std::string hostDll;
		};

		std::unordered_map<std::string, std::vector<HostMapping>> mappings_;


	};
}