#pragma once


#include <optional>
#include <string>
#include <string_view>

namespace ApiSet {
	bool isContract(std::string_view name);

	std::optional<std::wstring> resolve(std::wstring_view contract, std::wstring_view parentModule = {});
}

